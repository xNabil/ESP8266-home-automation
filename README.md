# ESP8266 4-Channel WiFi Smart Home Controller

This project turns an ESP8266 board into a smart controller for your home, allowing you to control up to four devices (like lights, fans, or motors) from your phone or computer.

It hosts its own **web interface**, meaning you don't need any special app. Once it's connected to your home WiFi, **any device on that same WiFi network** can access the control panel by simply visiting the device's IP address in a web browser.

It also works with your existing physical wall switches, so you can control your devices from the wall or from your phone.

## How It Works in 3 Simple Steps

1.  **Web Control:** The ESP8266 hosts a website. When you visit this website from your phone or computer, you can see buttons to turn your devices ON or OFF.
2.  **Physical Control:** You can also wire in your normal wall switches. The ESP8266 detects when you flip a switch and toggles the device, keeping everything in sync.
3.  **Easy Setup:** The first time you turn it on, it creates its own WiFi network called `HomeSetup`. You connect to this, and it automatically shows you a page where you can teach it your home WiFi password. After that, it just connects automatically.

## Main Features

* `✅` **Control From Any Device:** Works with any device that has a web browser (iPhone, Android, PC, Mac, etc.) as long as it's on your home WiFi network.
* `🏠` **Mobile-Friendly Web Page:** The control page is simple and designed to look good on a phone.
* `🔄` **Works with Wall Switches:** You get the "smart" web control without losing the "dumb" physical switch control.
* `🧠` **Remembers Your Settings:** If the power goes out, the device will remember if your lights were on or off and return to that state when the power comes back. It also securely saves your WiFi password.
* `✏️` **Customize Your Devices:** You can easily rename your devices (e.g., "Light" to "Bedroom Lamp") and change their icons (e.g., "💡" to "🛋️").
* `🔧` **Easy First-Time Setup:** A built-in "Access Point" mode (`HomeSetup`) with a captive portal makes setup a breeze, even for non-technical users.
* `💡` **Status Light:** The small blue light on the ESP8266 blinks to tell you what it's doing:
    * **Fast Blink:** In Setup Mode (`HomeSetup` is active).
    * **Slow Blink:** Trying to connect to your WiFi.
    * **Slow Pulse ("Heartbeat"):** Successfully connected to your WiFi and running normally.
* `🔒` **Easy Reset:** If you change your WiFi password, you can easily reset the device. You can either use the "Factory Reset" button in the web settings or press and **hold the physical "Reset" button (wired to D3) for 3 seconds**.

## How to Use

### 1. First-Time Setup

1.  After wiring everything up, plug in the ESP8266.
2.  On your phone or computer, look for a new WiFi network called **`HomeSetup`** and connect to it.
3.  Your device should automatically open the setup page. If not, open your web browser (like Chrome or Safari) and go to `http://192.168.4.1`.
4.  You will see the WiFi settings. Select your home WiFi network, type in your password, and click **"Save & Reboot"**.
5.  The device will restart and connect to your home WiFi. The webpage will tell you what its new IP address is (e.g., `192.168.1.50`). **Write this IP address down!**

### 2. Controlling Your Devices

1.  Make sure your phone or computer is connected to your **normal home WiFi**.
2.  Open your web browser.
3.  In the address bar, type the IP address you wrote down (e.g., `192.168.1.50`).
4.  You will see the control panel with buttons for "Light", "Fan", etc. Just tap a button to turn a device ON or OFF.

### 3. How to Customize Names and Icons

1.  Open the control panel in your browser (like in step 2 above).
2.  **Press and hold your finger on the empty background** of the page for 3 seconds.
3.  "Edit Mode" will activate, and you will see a "Cancel" button and small ✏️ "pen" icons next to each device name.
4.  Tap the ✏️ icon for the device you want to change.
5.  A popup will appear. Type the new name and a new icon (emojis work great!) and click **"Save"**.
6.  Click the "Cancel" button at the bottom to exit Edit Mode. Your new names are now saved.

## Hardware Needed

* **ESP8266 Board:** A NodeMCU or Wemos D1 Mini is perfect.
* **4-Channel Relay Module:** A board with 4 relays to safely switch your devices.
* **Physical Switches:** 4 standard wall switches (toggle or momentary).
* **Reset Button:** 1 small push-button for the factory reset.
* **Power Supply:** A good 5V power supply (like a phone charger) for the ESP8266 and relays.

## Wiring Guide (Pinout)

| Function | ESP8266 Pin (NodeMCU) | Connects To... |
| :--- | :--- | :--- |
| **Relay Outputs** | | |
| Relay 1 (Light) | `D6` (GPIO 12) | Relay "IN1" |
| Relay 2 (Fan) | `D7` (GPIO 13) | Relay "IN2" |
| Relay 3 (Motor) | `D8` (GPIO 15) | Relay "IN3" |
| Relay 4 (Light 2) | `D5` (GPIO 14) | Relay "IN4" |
| **Switch Inputs** | | |
| Switch 1 (Light) | `D1` (GPIO 5) | One side of wall switch. Other side to GND. |
| Switch 2 (Fan) | `D2` (GPIO 4) | One side of wall switch. Other side to GND. |
| Switch 3 (Motor) | `TX` (GPIO 1) | One side of wall switch. Other side to GND. |
| Switch 4 (Light 2) | `RX` (GPIO 3) | One side of wall switch. Other side to GND. |
| **System** | | |
| Factory Reset | `D3` (GPIO 0) | One side of reset button. Other side to GND. |
| Status LED | `D4` (GPIO 2) | This is the **built-in LED** on the board. |
