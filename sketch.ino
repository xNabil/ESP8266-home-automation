#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
// Pin definitions
const int lightPin = 12;  // D6
const int fanPin = 13;    // D7
const int motorPin = 15;  // D8
const int light2Pin = 14; // D5
const int ledPin = 2;     // Built-in LED (active LOW: LOW to light up)
const int resetPin = 0;   // D3 (GPIO0) for reset button (connect button to GND, internal pullup)
// Switch input pins
const int lightSwitchPin = 5;   // D1
const int fanSwitchPin = 4;     // D2
const int motorSwitchPin = 1;   // TX (GPIO1)
const int light2SwitchPin = 3;  // RX (GPIO3)
// LED defines
#define LED_ON LOW
#define LED_OFF HIGH
// Debounce
const unsigned long debounceDelay = 50;
// Switch debounce variables for light
int lastLightReading = HIGH;
unsigned long lastLightDebounceTime = 0;
int lightSwitchState = HIGH;
bool lastLightStable = HIGH;
// Switch debounce variables for fan
int lastFanReading = HIGH;
unsigned long lastFanDebounceTime = 0;
int fanSwitchState = HIGH;
bool lastFanStable = HIGH;
// Switch debounce variables for motor
int lastMotorReading = HIGH;
unsigned long lastMotorDebounceTime = 0;
int motorSwitchState = HIGH;
bool lastMotorStable = HIGH;
// Switch debounce variables for light2
int lastLight2Reading = HIGH;
unsigned long lastLight2DebounceTime = 0;
int light2SwitchState = HIGH;
bool lastLight2Stable = HIGH;
// EEPROM settings
#define EEPROM_SIZE 512
#define CONFIG_ADDR 0
struct Config {
  char ssid[32];
  char password[64];
  char staticIP[16];  // e.g., "192.168.1.100"
  bool hasConfig;
  char deviceNames[4][32];
  char deviceIcons[4][16];
  char loginPassword[32];
  bool persistentLogin;
};
Config config;
const int STATES_ADDR = sizeof(Config);
// Server and DNS
ESP8266WebServer server(80);
DNSServer dnsServer;
// WiFi settings
const char* apSSID = "HomeSetup";
IPAddress apIP(192, 168, 4, 1);
// Blink timing for non-connected states
unsigned long lastBlink = 0;
int blinkInterval = 3000;  // Default 3s for disconnected
bool ledState = false;
// Pulse timing for connected state
unsigned long ledTimer = 0;
int ledPhase = 0;  // 0: off (10s), 1: on (0.5s)
// Connection state
bool isConnected = false;
bool isAPMode = false;
int connectAttempts = 0;
const int maxConnectAttempts = 20;  // ~10 min at 30s intervals
// Reset button timing
unsigned long pressStart = 0;
const unsigned long resetHoldTime = 3000;  // 3 seconds
// HTML and CSS (hardcoded for simplicity, no SPIFFS needed)
const char* loginPage = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login - Home Automation</title>
    <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🏠</text></svg>">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background-color: #000; 
            color: #fff; 
            margin: 0; 
            padding: 20px; 
            display: flex; 
            flex-direction: column; 
            align-items: center; 
            justify-content: center;
            height: 100vh;
        }
        h1 { color: #0096FF; text-align: center; }
        #loginForm { 
            background-color: #111; 
            padding: 20px; 
            border-radius: 10px; 
            width: 300px; 
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        input { 
            width: 100%; 
            padding: 10px; 
            margin: 10px 0; 
            border: none; 
            border-radius: 5px; 
            background-color: #333; 
            color: #fff; 
            box-sizing: border-box;
        }
        button[type="submit"] { 
            background-color: #0096FF; 
            color: #000; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            width: 100%; 
            margin-top: 10px;
        }
        .password-container {
            position: relative;
            width: 100%;
        }
        .password-container input {
            width: 100%;
            box-sizing: border-box;
            margin: 10px 0;
        }
        .password-container span {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            cursor: pointer;
        }
        label {
            margin: 10px 0;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 5px;
        }
        #errorMsg { 
            color: #ff0000; 
            margin-bottom: 10px; 
            text-align: center;
            display: none;
        }
        button:focus,
        div:focus,
        a:focus {
          outline: none;
        }
        button, div, a {
          -webkit-tap-highlight-color: transparent;
        }
    </style>
</head>
<body>
    <h1>Login</h1>
    <div id="errorMsg">Wrong credentials!</div>
    <form id="loginForm" method="POST" action="/login">
        <div class="password-container">
            <input type="password" name="password" id="passInput" placeholder="Password" required>
            <span id="togglePassword"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></span>
        </div>
        <label>
            <input type="checkbox" name="remember"> Keep me logged in
        </label>
        <button type="submit">Login</button>
    </form>
    <script>
        const params = new URLSearchParams(window.location.search);
        if (params.get('error')) {
            document.getElementById('errorMsg').style.display = 'block';
        }
        document.getElementById('togglePassword').addEventListener('click', function() {
            var pass = document.getElementById('passInput');
            if (pass.type === 'password') {
                pass.type = 'text';
                this.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M17.94 17.94A10.94 10.94 0 0 1 12 20c-7 0-11-8-11-8a19.92 19.92 0 0 1 5.06-5.94"/><path d="M1 1l22 22"/><path d="M14.12 14.12a3 3 0 0 1-4.24-4.24"/></svg>';
            } else {
                pass.type = 'password';
                this.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
            }
        });
    </script>
</body>
</html>
)rawliteral";

const char* controlPage = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Home Automation Control</title>
    <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🏠</text></svg>">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background-color: #000; 
            color: #fff; 
            margin: 0; 
            padding: 20px; 
            display: flex; 
            flex-direction: column; 
            align-items: center; 
        }
        h1 { color: #0096FF; }
        .toggle { 
            display: flex; 
            flex-direction: column; 
            align-items: center; 
            margin: 10px; 
        }
        .toggle label {
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .toggle-btn { 
            width: 200px; 
            height: 60px; 
            border: none; 
            border-radius: 30px; 
            font-size: 18px; 
            font-weight: bold; 
            cursor: pointer; 
            transition: background-color 0.3s; 
            display: flex; 
            align-items: center; 
            justify-content: center; 
            gap: 10px; 
        }
        .off { background-color: #333; color: #fff; }
        .on { background-color: #0096FF; color: #000; }
        .status { 
            margin-top: 20px; 
            padding: 10px; 
            border-radius: 10px; 
            text-align: center; 
        }
        .online { background-color: #0096FF; color: #000; }
        .offline { background-color: #ff0000; color: #fff; }
        #configSection { 
            margin-top: 30px; 
            padding: 20px; 
            background-color: #111; 
            border-radius: 10px; 
            width: 300px; 
        }
        input { 
            width: 100%; 
            padding: 10px; 
            margin: 10px 0; 
            border: none; 
            border-radius: 5px; 
            background-color: #333; 
            color: #fff; 
            box-sizing: border-box;
        }
        button[type="submit"] { 
            background-color: #0096FF; 
            color: #000; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            width: 100%; 
        }
        .reset-btn { 
            background-color: #ff0000; 
            color: #fff; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            margin-top: 10px; 
            width: 100%; 
        }
        .logout-btn { 
            background-color: #ff0000; 
            color: #fff; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            margin-top: 10px; 
            width: 100%; 
        }
        .config-btn { 
            background-color: #0096FF; 
            color: #000; 
            padding: 10px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            margin-top: 10px; 
            width: 100%; 
        }
        /* Icons using Unicode */
        .icon { font-size: 20px; }
        #menuBtn {
            position: fixed;
            left: 20px;
            top: 20px;
            background: none;
            border: none;
            font-size: 30px;
            color: #fff;
            cursor: pointer;
            display: none;
            z-index: 2;
        }
        #sidebar {
            position: fixed;
            left: -250px;
            top: 0;
            width: 250px;
            height: 100%;
            background: #111;
            transition: left 0.3s;
            padding: 60px 20px 20px 20px;
            box-sizing: border-box;
            z-index: 1;
            display: flex;
            flex-direction: column;
        }
        #sidebar h2 {
            color: #0096FF;
        }
        .password-container {
            position: relative;
            width: 100%;
        }
        .password-container input {
            width: 100%;
            box-sizing: border-box;
            margin: 10px 0;
        }
        .password-container span {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            cursor: pointer;
        }
        .pass-toggle {
            position: absolute;
            right: 10px;
            top: 50%;
            transform: translateY(-50%);
            cursor: pointer;
        }
        .edit-pen {
            display: none;
            font-size: 20px;
            margin-left: 10px;
            cursor: pointer;
        }
        body.edit-mode .edit-pen {
            display: inline;
        }
        #editModal {
            display: none;
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: #111;
            padding: 20px;
            border-radius: 10px;
            z-index: 3;
            color: #fff;
        }
        #editModal input {
            background: #333;
            color: #fff;
        }
        #editModal button {
            margin: 10px 5px 0 0;
            padding: 10px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        #editModal #saveBtn {
            background: #0096FF;
            color: #000;
        }
        #editModal #cancelBtn {
            background: #ff0000;
            color: #fff;
        }
        #sidebar-spacer {
            flex-grow: 1;
        }
        button:focus,
        div:focus,
        a:focus {
          outline: none;
        }
        button, div, a {
          -webkit-tap-highlight-color: transparent;
        }
    </style>
</head>
<body>
    <button id="menuBtn">☰</button>
    <div id="sidebar">
        <h2>Settings</h2>
        <button class="config-btn" id="wifiBtn" onclick="toggleWifiConfig()">WiFi Configuration</button>
        <div id="wifiConfigForm" style="display: none;">
            <form action="/save" method="POST">
                <input type="text" name="ssid" placeholder="WiFi SSID" required>
                <div class="password-container">
                    <input type="password" id="password" name="password" placeholder="WiFi Password">
                    <span id="togglePassword"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></span>
                </div>
                <input type="text" name="staticIP" placeholder="Static IP (e.g., 192.168.1.100, optional)">
                <button type="submit">Save & Reboot</button>
            </form>
        </div>
        <button class="config-btn" id="loginBtn" onclick="toggleLoginConfig()">Login Configuration</button>
        <div id="loginConfigForm" style="display: none;">
            <form action="/save_login" method="POST">
                <div class="password-container">
                    <input type="password" name="currentPass" placeholder="Enter Current Password" required>
                    <span class="pass-toggle" onclick="toggleThisPass(this)"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></span>
                </div>
                <div class="password-container">
                    <input type="password" name="newPass" placeholder="New Login Password" required>
                    <span class="pass-toggle" onclick="toggleThisPass(this)"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></span>
                </div>
                <button type="submit">Save</button>
            </form>
        </div>
        <div id="sidebar-spacer"></div>
        <button class="reset-btn" id="resetBtn" onclick="toggleResetConfig()">Factory Reset</button>
        <div id="resetConfigForm" style="display: none;">
            <p>Enter current password to confirm factory reset.</p>
            <div class="password-container">
                <input type="password" id="resetPass" placeholder="Current Password" required>
                <span class="pass-toggle" onclick="toggleThisPass(this)"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></span>
            </div>
            <br><button type="button" class="reset-btn" onclick="performReset()">Confirm Reset</button></br>
            <br><button type="button" style="background-color: #0096FF; color: #fff; padding: 10px; border: none; border-radius: 5px; cursor: pointer; width: 100%;" onclick="toggleResetConfig()">Cancel</button></br>
        </div>
        <button class="logout-btn" onclick="logout()">Logout</button>
    </div>
    <h1>HomeIQ</h1>
    
<div class="toggle">
    <label>Light</label>
    <button class="toggle-btn off" id="lightBtn" onclick="toggle('light')">
        <span class="icon"></span> OFF
    </button>
</div>

<div class="toggle">
    <label>Light-2</label>
    <button class="toggle-btn off" id="light2Btn" onclick="toggle('light2')">
        <span class="icon"></span> OFF
    </button>
</div>

<div class="toggle">
    <label>Fan</label>
    <button class="toggle-btn off" id="fanBtn" onclick="toggle('fan')">
        <span class="icon"></span> OFF
    </button>
</div>

<div class="toggle">
    <label>Motor</label>
    <button class="toggle-btn off" id="motorBtn" onclick="toggle('motor')">
        <span class="icon"></span> OFF
    </button>
</div>

<div id="status" class="status offline">Device is offline</div>

<div id="editModal">
    <h3>Edit Device</h3>
    <input id="newName" placeholder="Rename Title">
    <input id="newIcon" placeholder="Change icon (emoji)">
    <button id="saveBtn" onclick="saveEdit()">Save</button>
    <button id="cancelBtn" onclick="cancelEdit()">Cancel</button>
</div>

<script>
    let customs = [];
    let currentIdx = -1;
    let isEditMode = false;
    let pressTimer;
    let lastUserActivity = Date.now();

    document.addEventListener('click', () => { lastUserActivity = Date.now(); });
    document.addEventListener('touchstart', () => { lastUserActivity = Date.now(); });

    function loadCustoms() {
        fetch('/get_custom')
            .then(response => response.json())
            .then(data => {
                customs = data;
                updateLabelsAndButtons();
                addPens();
            });
    }

    function updateLabelsAndButtons() {
        const pins = ['light', 'light2', 'fan', 'motor'];
        const labels = document.querySelectorAll('.toggle label');
        pins.forEach((pin, idx) => {
            if (labels[idx].firstChild.nodeType === Node.TEXT_NODE) {
                labels[idx].firstChild.textContent = customs[idx].name;
            }
            const btn = document.getElementById(pin + 'Btn');
            const state = btn.classList.contains('on') ? 'ON' : 'OFF';
            btn.innerHTML = '<span class="icon">' + customs[idx].icon + '</span> ' + state;
        });
    }

    function addPens() {
        const labels = document.querySelectorAll('.toggle label');
        const pins = ['light', 'light2', 'fan', 'motor'];
        labels.forEach((lab, idx) => {
            if (!lab.querySelector('.edit-pen')) {
                const textNode = document.createTextNode(customs[idx].name || pins[idx].charAt(0).toUpperCase() + pins[idx].slice(1));
                lab.innerHTML = ''; // clear
                lab.appendChild(textNode);
                let pen = document.createElement('span');
                pen.className = 'edit-pen';
                pen.innerHTML = '✏️';
                pen.onclick = () => editDevice(idx);
                lab.appendChild(pen);
            }
        });
    }

    function editDevice(idx) {
        currentIdx = idx;
        document.getElementById('newName').value = customs[idx].name;
        document.getElementById('newIcon').value = customs[idx].icon;
        document.getElementById('editModal').style.display = 'block';
    }

    function saveEdit() {
        const newName = document.getElementById('newName').value;
        const newIcon = document.getElementById('newIcon').value;
        fetch(`/save_device?idx=${currentIdx}&name=${encodeURIComponent(newName)}&icon=${encodeURIComponent(newIcon)}`)
            .then(() => {
                customs[currentIdx].name = newName;
                customs[currentIdx].icon = newIcon;
                updateLabelsAndButtons();
                cancelEdit();
            });
    }

    function cancelEdit() {
        document.getElementById('editModal').style.display = 'none';
    }

    function updateStatus() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                if (!data.persistent && (Date.now() - lastUserActivity > 30000)) {
                    fetch('/logout').then(() => {
                        window.location.href = '/';
                    });
                    return;
                }
                document.getElementById('status').innerHTML = data.connected ? 'Device is online' : 'Device is offline';
                document.getElementById('status').className = 'status ' + (data.connected ? 'online' : 'offline');
                document.getElementById('menuBtn').style.display = 'block';
                if (data.connected) {
                    document.getElementById('wifiBtn').style.display = 'block';
                    var sidebar = document.getElementById('sidebar');
                    if (sidebar.style.left !== '0px') {
                        document.getElementById('wifiConfigForm').style.display = 'none';
                    }
                } else {
                    document.getElementById('wifiBtn').style.display = 'block';
                    var sidebar = document.getElementById('sidebar');
                    if (sidebar.style.left !== '0px') {
                        document.getElementById('wifiConfigForm').style.display = 'none';
                    }
                }
                // Update toggles
                const pins = ['light', 'light2', 'fan', 'motor'];
                pins.forEach((pin, idx) => {
                    const btn = document.getElementById(pin + 'Btn');
                    const state = data[pin];
                    btn.innerHTML = '<span class="icon">' + (customs[idx] ? customs[idx].icon : getDefaultIcon(pin)) + '</span> ' + (state ? 'ON' : 'OFF');
                    btn.className = 'toggle-btn ' + (state ? 'on' : 'off');
                });
            });
    }
    
    function getDefaultIcon(pin) {
        if (pin === 'light' || pin === 'light2') return '💡';
        if (pin === 'fan') return '🌀';
        if (pin === 'motor') return '⚙️';
        return '';
    }
    
    function toggle(pin) {
        const currentState = document.getElementById(pin + 'Btn').classList.contains('on');
        const newState = !currentState ? 1 : 0;
        fetch('/set?pin=' + pin + '&state=' + newState)
            .then(() => updateStatus());
    }
    
    function resetConfig() {
        if (confirm('Are you sure? This will clear WiFi settings and reboot.')) {
            fetch('/clear')
                .then(() => location.reload());
        }
    }
    
    function toggleWifiConfig() {
        let form = document.getElementById('wifiConfigForm');
        if (form.style.display === 'none' || form.style.display === '') {
            form.style.display = 'block';
            fetch('/get_config')
                .then(response => response.json())
                .then(data => {
                    document.querySelector('#wifiConfigForm input[name="ssid"]').value = data.ssid;
                    document.querySelector('#wifiConfigForm input[name="staticIP"]').value = data.staticIP;
                });
        } else {
            form.style.display = 'none';
        }
    }

    function toggleLoginConfig() {
        let form = document.getElementById('loginConfigForm');
        if (form.style.display === 'none' || form.style.display === '') {
            form.style.display = 'block';
        } else {
            form.style.display = 'none';
        }
    }

    function toggleResetConfig() {
        let form = document.getElementById('resetConfigForm');
        if (form.style.display === 'none' || form.style.display === '') {
            form.style.display = 'block';
            document.getElementById('resetBtn').style.display = 'none';
        } else {
            form.style.display = 'none';
            document.getElementById('resetBtn').style.display = 'block';
        }
    }

    function performReset() {
        let pass = document.getElementById('resetPass').value;
        if (!pass) {
            alert('Please enter password');
            return;
        }
        fetch('/reset_confirm', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: 'password=' + encodeURIComponent(pass)
        }).then(response => {
            if (response.ok) {
                fetch('/clear').then(() => location.reload());
            } else {
                alert('Wrong password');
                document.getElementById('resetPass').value = '';
            }
        });
    }

    function toggleThisPass(span) {
        let input = span.parentElement.querySelector('input');
        if (input.type === 'password') {
            input.type = 'text';
            span.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M17.94 17.94A10.94 10.94 0 0 1 12 20c-7 0-11-8-11-8a19.92 19.92 0 0 1 5.06-5.94"/><path d="M1 1l22 22"/><path d="M14.12 14.12a3 3 0 0 1-4.24-4.24"/></svg>';
        } else {
            input.type = 'password';
            span.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
        }
    }

    function logout() {
        if (confirm('Are you sure you want to log out?')) {
            window.location.href = '/logout';
        }
    }
    
    document.getElementById('menuBtn').addEventListener('click', function() {
        var sidebar = document.getElementById('sidebar');
        if (sidebar.style.left === '0px') {
            sidebar.style.left = '-250px';
        } else {
            sidebar.style.left = '0px';
        }
    });
    
    document.getElementById('togglePassword').addEventListener('click', function() {
        var password = document.getElementById('password');
        if (password.type === 'password') {
            password.type = 'text';
            this.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M17.94 17.94A10.94 10.94 0 0 1 12 20c-7 0-11-8-11-8a19.92 19.92 0 0 1 5.06-5.94"/><path d="M1 1l22 22"/><path d="M14.12 14.12a3 3 0 0 1-4.24-4.24"/></svg>';
        } else {
            password.type = 'password';
            this.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="cursor:pointer;"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
        }
    });

    // Long press detection
    document.body.addEventListener('mousedown', function(e) {
        if (e.target === document.body) {
            pressTimer = window.setTimeout(toggleEditMode, 2000);
        }
    });

    document.body.addEventListener('mouseup', function() {
        clearTimeout(pressTimer);
    });

    document.body.addEventListener('touchstart', function(e) {
        if (e.target === document.body) {
            pressTimer = window.setTimeout(toggleEditMode, 2000);
        }
    }, {passive: false});

    document.body.addEventListener('touchend', function() {
        clearTimeout(pressTimer);
    });

    function toggleEditMode() {
        isEditMode = !isEditMode;
        if (isEditMode) {
            document.body.classList.add('edit-mode');
            // Create and insert cancel button
            let cancelBtn = document.createElement('button');
            cancelBtn.id = 'cancelEditMode';
            cancelBtn.textContent = 'Cancel Edit Mode';
            cancelBtn.style.backgroundColor = '#ff0000';
            cancelBtn.style.color = '#fff';
            cancelBtn.style.padding = '10px';
            cancelBtn.style.border = 'none';
            cancelBtn.style.borderRadius = '5px';
            cancelBtn.style.cursor = 'pointer';
            cancelBtn.style.marginBottom = '10px';
            cancelBtn.style.width = '200px';
            cancelBtn.onclick = toggleEditMode;
            let statusDiv = document.getElementById('status');
            statusDiv.parentNode.insertBefore(cancelBtn, statusDiv);
        } else {
            document.body.classList.remove('edit-mode');
            // Remove cancel button
            let cancelBtn = document.getElementById('cancelEditMode');
            if (cancelBtn) {
                cancelBtn.remove();
            }
        }
        updateLabelsAndButtons();
    }
    
    // Update on load and every 5s
    updateStatus();
    loadCustoms();
    setInterval(updateStatus, 5000);
</script></body>
</html>
)rawliteral";

const char* connectPage = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Connecting...</title>
    <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🏠</text></svg>">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background-color: #000; 
            color: #fff; 
            margin: 0; 
            padding: 20px; 
            display: flex; 
            flex-direction: column; 
            align-items: center; 
            justify-content: center;
            height: 100vh;
        }
        h1 { color: #0096FF; }
        p { font-size: 18px; }
        button:focus,
        div:focus,
        a:focus {
          outline: none;
        }
        button, div, a {
          -webkit-tap-highlight-color: transparent;
        }
    </style>
</head>
<body>
    <h1>Connecting to WiFi...</h1>
    <p>Please wait while we connect to your network.</p>
    <div id="popup" style="display:none; position:fixed; top:50%; left:50%; transform:translate(-50%,-50%); background:#111; color:#fff; padding:20px; border-radius:10px; text-align:center; z-index:10;">
        <p>Connected! Visit <a id="ipLink" href="#" style="color:#0096FF;" target="_blank"></a> to control the device.
AP will turn off automatically in 10 seconds.</p>
    </div>
    <script>
        let interval = setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    if (data.connected && data.ip && data.ip !== '0.0.0.0') {
                        let ipUrl = 'http://' + data.ip;
                        document.getElementById('ipLink').href = ipUrl;
                        document.getElementById('ipLink').textContent = ipUrl;
                        document.getElementById('popup').style.display = 'block';
                        clearInterval(interval);
                        setTimeout(() => {
                            finishSetup();
                        }, 10000);
                    }
                });
        }, 2000);

    function finishSetup() {
        fetch('/finish_setup')
            .then(() => {
                alert('AP will stop now. Please connect to your home WiFi and visit the provided IP.');
            });
    }
</script></body>
</html>
)rawliteral";

bool isAuthenticated() {
  String cookieStr = server.header("Cookie");
  bool cookieAuth = (cookieStr.indexOf("auth=valid") != -1);
  return config.persistentLogin || cookieAuth;
}

String toStringIp(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}
void setDefaults() {
  strcpy(config.deviceNames[0], "Light");
  strcpy(config.deviceNames[1], "Light-2");
  strcpy(config.deviceNames[2], "Fan");
  strcpy(config.deviceNames[3], "Motor");
  strcpy(config.deviceIcons[0], "💡");
  strcpy(config.deviceIcons[1], "💡");
  strcpy(config.deviceIcons[2], "🌀");
  strcpy(config.deviceIcons[3], "⚙️");
  strcpy(config.loginPassword, "user");
  config.persistentLogin = false;
}
void saveStates() {
  EEPROM.write(STATES_ADDR, digitalRead(lightPin));
  EEPROM.write(STATES_ADDR + 1, digitalRead(fanPin));
  EEPROM.write(STATES_ADDR + 2, digitalRead(motorPin));
  EEPROM.write(STATES_ADDR + 3, digitalRead(light2Pin));
  EEPROM.commit();
}
void loadConfig() {
  EEPROM.get(CONFIG_ADDR, config);
  if (strlen(config.ssid) == 0 || !config.hasConfig) {
    config.hasConfig = false;
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.staticIP, "");
  }
  if (strlen(config.deviceNames[0]) == 0) {
    setDefaults();
  }
  if (strlen(config.loginPassword) == 0) {
    strcpy(config.loginPassword, "user");
  }
}
void saveConfig() {
  if (strlen(config.deviceNames[0]) == 0) {
    setDefaults();
  }
  EEPROM.put(CONFIG_ADDR, config);
  EEPROM.commit();
  Serial.println("Config saved.");
}
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  if (strlen(config.staticIP) > 0) {
    IPAddress ip;
    ip.fromString(config.staticIP);
    IPAddress gateway(192, 168, 1, 1);  // Assume common home router
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);
    Serial.println("Static IP set: " + String(config.staticIP));
  } else {
    WiFi.config(0U, 0U, 0U);  // DHCP
  }  WiFi.begin(config.ssid, config.password);
  connectAttempts = 0;
  isConnected = false;
  blinkInterval = 3000;  // Disconnected blink
}
void startAPMode() {
  isAPMode = true;
  isConnected = false;
  blinkInterval = 500;  // Fast blink for AP  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, "11223344");
  dnsServer.start(53, "*", apIP);  Serial.println("AP started. SSID: " + String(apSSID) + ", IP: " + WiFi.softAPIP().toString());
}
void clearConfigAndRestart() {
  config.hasConfig = false;
  strcpy(config.ssid, "");
  strcpy(config.password, "");
  strcpy(config.staticIP, "");
  setDefaults(); // Reset devices too
  EEPROM.write(STATES_ADDR, 0);
  EEPROM.write(STATES_ADDR + 1, 0);
  EEPROM.write(STATES_ADDR + 2, 0);
  EEPROM.write(STATES_ADDR + 3, 0);
  saveConfig();
  Serial.println("Config cleared. Rebooting to AP mode...");
  delay(1000);
  ESP.restart();
}
void handleResetButton() {
  bool pressed = (digitalRead(resetPin) == LOW);  if (pressed && pressStart == 0) {
    pressStart = millis();
    Serial.println("Reset button pressed...");
  } else if (!pressed && pressStart != 0) {
    unsigned long pressDuration = millis() - pressStart;
    if (pressDuration >= resetHoldTime) {
      Serial.println("Long press detected (" + String(pressDuration) + "ms). Resetting config...");
      clearConfigAndRestart();
    } else {
      Serial.println("Short press ignored (" + String(pressDuration) + "ms).");
    }
    pressStart = 0;
  }
}
void setupServer() {
  // Status endpoint
  server.on("/status", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    String json = "{";
    json += "\"connected\":" + String(isConnected ? "true" : "false") + ",";
    json += "\"ip\":\"" + (isConnected ? WiFi.localIP().toString() : "0.0.0.0") + "\",";
    json += "\"light\":" + String(digitalRead(lightPin)) + ",";
    json += "\"light2\":" + String(digitalRead(light2Pin)) + ",";
    json += "\"fan\":" + String(digitalRead(fanPin)) + ",";
    json += "\"motor\":" + String(digitalRead(motorPin)) + ",";
    json += "\"persistent\":" + String(config.persistentLogin ? "true" : "false");
    json += "}";
    server.send(200, "application/json; charset=utf-8", json);
  });  // Get config endpoint (without password)
  server.on("/get_config", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    String json = "{";
    json += "\"ssid\":\"" + String(config.ssid) + "\",";
    json += "\"staticIP\":\"" + String(config.staticIP) + "\"";
    json += "}";
    server.send(200, "application/json; charset=utf-8", json);
  });  // Set endpoint
  server.on("/set", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    if (server.hasArg("pin") && server.hasArg("state")) {
      String pin = server.arg("pin");
      int state = server.arg("state").toInt();
      if (pin == "light") digitalWrite(lightPin, state);
      else if (pin == "light2") digitalWrite(light2Pin, state);
      else if (pin == "fan") digitalWrite(fanPin, state);
      else if (pin == "motor") digitalWrite(motorPin, state);
      saveStates();
      server.send(200, "text/plain; charset=utf-8", "OK");
    } else {
      server.send(400, "text/plain; charset=utf-8", "Bad Request");
    }
  });  // Login endpoint
  server.on("/login", HTTP_POST, []() {
    String pass = server.arg("password");
    String rem = server.arg("remember");
    bool remember = (rem == "on" || rem == "1");
    if (pass == String(config.loginPassword)) {
      if (remember) {
        config.persistentLogin = true;
        saveConfig();
        server.sendHeader("Set-Cookie", "auth=valid; Path=/; Max-Age=31536000; HttpOnly");
      } else {
        server.sendHeader("Set-Cookie", "auth=valid; Path=/; HttpOnly");
      }
      server.sendHeader("Location", "/");
      server.send(302);
    } else {
      server.sendHeader("Location", "/?error=1");
      server.send(302);
    }
  });  // Logout endpoint
  server.on("/logout", []() {
    config.persistentLogin = false;
    saveConfig();
    server.sendHeader("Set-Cookie", "auth=; Path=/; Max-Age=0; HttpOnly");
    server.sendHeader("Location", "/");
    server.send(302);
  });  // Save login password
  server.on("/save_login", HTTP_POST, []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    String currentPass = server.arg("currentPass");
    String newPass = server.arg("newPass");
    if (currentPass == String(config.loginPassword)) {
      strncpy(config.loginPassword, newPass.c_str(), 31);
      config.loginPassword[31] = '\0';
      config.persistentLogin = false;
      saveConfig();
      server.sendHeader("Location", "/");
      server.send(302);
    } else {
      server.sendHeader("Location", "/?loginerror=1");
      server.send(302);
    }
  });  // Reset confirm endpoint
  server.on("/reset_confirm", HTTP_POST, []() {
    if(!isAuthenticated()) {
      server.send(401, "text/plain", "Unauthorized");
      return;
    }
    String pass = server.arg("password");
    if (pass == String(config.loginPassword)) {
      server.send(200, "text/plain", "OK");
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
  });  // Save config
  server.on("/save", HTTP_POST, []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    if (server.hasArg("ssid")) {
      strncpy(config.ssid, server.arg("ssid").c_str(), 31);
      config.ssid[31] = '\0';
    }
    if (server.hasArg("password")) {
      strncpy(config.password, server.arg("password").c_str(), 63);
      config.password[63] = '\0';
    }
    if (server.hasArg("staticIP")) {
      strncpy(config.staticIP, server.arg("staticIP").c_str(), 15);
      config.staticIP[15] = '\0';
    }
    config.hasConfig = true;
    saveConfig();
    server.sendHeader("Set-Cookie", "auth=valid; Path=/; HttpOnly");
    // Start connecting while keeping AP on
    WiFi.mode(WIFI_AP_STA);
    if (strlen(config.staticIP) > 0) {
      IPAddress ip;
      ip.fromString(config.staticIP);
      IPAddress gateway(192, 168, 1, 1);
      IPAddress subnet(255, 255, 255, 0);
      WiFi.config(ip, gateway, subnet);
    } else {
      WiFi.config(0U, 0U, 0U);
    }
    WiFi.begin(config.ssid, config.password);
    connectAttempts = 0;
    isConnected = false;
    blinkInterval = 3000;
    server.send(200, "text/html; charset=utf-8", connectPage);
  }, []() {
    // Handle file upload if needed, but not used here
  });  // Finish setup endpoint
  server.on("/finish_setup", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    server.send(200, "text/plain; charset=utf-8", "OK");
    WiFi.mode(WIFI_STA);
    isAPMode = false;
    dnsServer.stop();
  });  // Clear config (factory reset)
  server.on("/clear", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    clearConfigAndRestart();
  });  // Get custom device names and icons
  server.on("/get_custom", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    String json = "[";
    for (int i = 0; i < 4; i++) {
      json += "{\"name\":\"" + String(config.deviceNames[i]) + "\",\"icon\":\"" + String(config.deviceIcons[i]) + "\"}";
      if (i < 3) json += ",";
    }
    json += "]";
    server.send(200, "application/json; charset=utf-8", json);
  });  // Save device custom
  server.on("/save_device", []() {
    if(!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      return;
    }
    if (server.hasArg("idx") && server.hasArg("name") && server.hasArg("icon")) {
      int idx = server.arg("idx").toInt();
      if (idx >= 0 && idx < 4) {
        strncpy(config.deviceNames[idx], server.arg("name").c_str(), 31);
        config.deviceNames[idx][31] = '\0';
        strncpy(config.deviceIcons[idx], server.arg("icon").c_str(), 15);
        config.deviceIcons[idx][15] = '\0';
        saveConfig();
        server.send(200, "text/plain; charset=utf-8", "OK");
      } else {
        server.send(400, "text/plain; charset=utf-8", "Invalid index");
      }
    } else {
      server.send(400, "text/plain; charset=utf-8", "Bad Request");
    }
  });  // Root - control/setup page
  server.on("/", []() {
    if (isAuthenticated()) {
      server.send(200, "text/html; charset=utf-8", controlPage);
    } else {
      server.send(200, "text/html; charset=utf-8", loginPage);
    }
  });  // Captive portal redirects
  server.on("/generate_204", []() {  // Android
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");
  });
  server.on("/fwlink", []() {  // Microsoft
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");
  });  server.begin();
  Serial.println("HTTP server started");
}
void handleSwitches() {
  // Light switch
  int reading = digitalRead(lightSwitchPin);
  if (reading != lastLightReading) {
    lastLightDebounceTime = millis();
  }
  if ((millis() - lastLightDebounceTime) > debounceDelay) {
    if (reading != lightSwitchState) {
      lightSwitchState = reading;
      if (lightSwitchState != lastLightStable) {
        lastLightStable = lightSwitchState;
        // Toggle relay
        digitalWrite(lightPin, !digitalRead(lightPin));
        saveStates();
      }
    }
  }
  lastLightReading = reading;  // Fan switch
  reading = digitalRead(fanSwitchPin);
  if (reading != lastFanReading) {
    lastFanDebounceTime = millis();
  }
  if ((millis() - lastFanDebounceTime) > debounceDelay) {
    if (reading != fanSwitchState) {
      fanSwitchState = reading;
      if (fanSwitchState != lastFanStable) {
        lastFanStable = fanSwitchState;
        // Toggle relay
        digitalWrite(fanPin, !digitalRead(fanPin));
        saveStates();
      }
    }
  }
  lastFanReading = reading;  // Motor switch
  reading = digitalRead(motorSwitchPin);
  if (reading != lastMotorReading) {
    lastMotorDebounceTime = millis();
  }
  if ((millis() - lastMotorDebounceTime) > debounceDelay) {
    if (reading != motorSwitchState) {
      motorSwitchState = reading;
      if (motorSwitchState != lastMotorStable) {
        lastMotorStable = motorSwitchState;
        // Toggle relay
        digitalWrite(motorPin, !digitalRead(motorPin));
        saveStates();
      }
    }
  }
  lastMotorReading = reading;  // Light2 switch
  reading = digitalRead(light2SwitchPin);
  if (reading != lastLight2Reading) {
    lastLight2DebounceTime = millis();
  }
  if ((millis() - lastLight2DebounceTime) > debounceDelay) {
    if (reading != light2SwitchState) {
      light2SwitchState = reading;
      if (light2SwitchState != lastLight2Stable) {
        lastLight2Stable = light2SwitchState;
        // Toggle relay
        digitalWrite(light2Pin, !digitalRead(light2Pin));
        saveStates();
      }
    }
  }
  lastLight2Reading = reading;
}
void handleLED() {
  unsigned long now = millis();  if (isConnected) {
    // Pulse: 0.5s ON, 10s OFF
    if (ledPhase == 0) {  // OFF phase
      if (now - ledTimer > 10000) {
        digitalWrite(ledPin, LED_ON);
        ledTimer = now;
        ledPhase = 1;
      }
    } else {  // ON phase
      if (now - ledTimer > 500) {
        digitalWrite(ledPin, LED_OFF);
        ledTimer = now;
        ledPhase = 0;
      }
    }
  } else {
    // Simple blink for other states
    if (now - lastBlink > blinkInterval) {
      lastBlink = now;
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? LED_ON : LED_OFF);
    }// Update interval based on state
if (isAPMode) {
  blinkInterval = 500;    // Fast for AP/setup
} else {
  blinkInterval = 3000;   // Medium for disconnected/trying
}  }
}
void checkConnection() {
  static bool lastConnected = false;
  if (WiFi.status() == WL_CONNECTED && !isConnected) {
    isConnected = true;
    connectAttempts = 0;
    ledPhase = 0;  // Start OFF for pulse
    ledTimer = millis();
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
  } else if (WiFi.status() != WL_CONNECTED && isConnected) {
    isConnected = false;
    Serial.println("WiFi disconnected.");
  }
  if (isConnected != lastConnected) {
    lastConnected = isConnected;
  }
}
void setup() {
  // Initialize serial for debugging (optional, remove if not needed)
  Serial.begin(115200);
  Serial.println("Starting...");  // Initialize output pins
  pinMode(lightPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  pinMode(light2Pin, OUTPUT);  // Initialize input pins
  pinMode(lightSwitchPin, INPUT_PULLUP);
  pinMode(fanSwitchPin, INPUT_PULLUP);
  pinMode(motorSwitchPin, INPUT_PULLUP);
  pinMode(light2SwitchPin, INPUT_PULLUP);  // Initialize switch states
  lightSwitchState = digitalRead(lightSwitchPin);
  lastLightStable = lightSwitchState;
  lastLightReading = lightSwitchState;  fanSwitchState = digitalRead(fanSwitchPin);
  lastFanStable = fanSwitchState;
  lastFanReading = fanSwitchState;  motorSwitchState = digitalRead(motorSwitchPin);
  lastMotorStable = motorSwitchState;
  lastMotorReading = motorSwitchState;  light2SwitchState = digitalRead(light2SwitchPin);
  lastLight2Stable = light2SwitchState;
  lastLight2Reading = light2SwitchState;  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LED_OFF);  // Reset button: internal pullup
  pinMode(resetPin, INPUT_PULLUP);  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);  // Load config
  loadConfig();  // Load relay states (default to OFF if uninitialized)
  byte lightState = EEPROM.read(STATES_ADDR);
  if (lightState > 1) lightState = 0;
  digitalWrite(lightPin, lightState);  byte fanState = EEPROM.read(STATES_ADDR + 1);
  if (fanState > 1) fanState = 0;
  digitalWrite(fanPin, fanState);  byte motorState = EEPROM.read(STATES_ADDR + 2);
  if (motorState > 1) motorState = 0;
  digitalWrite(motorPin, motorState);  byte light2State = EEPROM.read(STATES_ADDR + 3);
  if (light2State > 1) light2State = 0;
  digitalWrite(light2Pin, light2State);  // Try to connect if config exists
  if (config.hasConfig) {
    Serial.println("Attempting WiFi connection...");
    connectToWiFi();
  } else {
    Serial.println("No config, starting AP mode...");
    startAPMode();
  }  // Setup server handlers
  setupServer();  Serial.println("Setup complete.");
}
void loop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();  // Handle physical switches
  handleSwitches();  // Handle reset button long press
  handleResetButton();  // Handle LED
  handleLED();  // Check connection status
  checkConnection();  // Retry connection if has config and not connected
  if (!isConnected && config.hasConfig && connectAttempts < maxConnectAttempts) {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 30000) {  // Every 30s
      lastAttempt = millis();
      connectAttempts++;
      Serial.println("Retry attempt: " + String(connectAttempts));
      WiFi.begin(config.ssid, config.password);
    }
  } else if (!isConnected && config.hasConfig && connectAttempts >= maxConnectAttempts) {
    Serial.println("Max attempts reached, starting AP mode...");
    startAPMode();
  }  delay(10);
}
