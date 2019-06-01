# Xiaomi-Thermostat-BLE
Code for Raspberry Pi that parses BLE-packages from Xiaomi Thermostat and produces data in JSON format to output. Doesn't establish any connection, works completelly passive, so the battery life remains the same. Works with both generations of sensors (Xiaomi and ClearGrass).

<p>Dependencies installation:<br />
sudo apt-get update<br />
sudo apt-get install libncurses5-dev</p>

<p>Compilation instructions:<br />
g++ -std=c++11 -o xiaomiscan xiaomiscan.cpp `pkg-config --cflags ncurses` -lbluetooth -lncurses</p>

<p>Example of output:<br />
<code>pi@hassbian:/opt/openhab/dev $ sudo ./xiaomiscan</code><br />
<code>Scanning....</code><br />
<code>{ "id": 16, "mac":"4c65a8d01747", "type": 13, "temperature": 23.8, "humidity": 37.6 }</code><br />
<code>{ "id": 25, "mac":"4c65a8d01747", "type": 4, "temperature": 23.8 }</code><br />
<code>{ "id": 26, "mac":"4c65a8d01747", "type": 13, "temperature": 23.9, "humidity": 37.3 }</code><br />
</code>
</p>
