 # software code and debugging
 <br/>This project involves the relevant programs for the ESP32C3 main controller, based on the ESP-IDF development environment, version 5.2.X，with C/C++.<br/>
 <br/>The test folder contains independent test code for each hardware module.<br/>
<br/>This Android application project is developed using the Android Studio development environment, with Java and XML as the programming languages, and Gradle version 7.1.2.<br/>

 
 ## The software component of the project consists of three main parts:
<br/> 1. firmware: runs on a microcontroller (ESP32C3 development board) and is used to capture, process, store and transmit data (ECG, temperature, battery voltage and accelerometer readings).
<br/>2. User-side application: runs on the patient's Android device and is used to receive, decode, store, visualize and upload data to the cloud. Running on the doctor's PC for receiving, decoding, visualizing and intelligent analysis.
<br/>3. Cloud application: running on the cloud server, used to save and analyze the patient's historical physiological data and device logs.

&emsp;&emsp;The implantable device communicates with the patient's cell phone via low-power Bluetooth (BLE).
The cell phone acts as a gateway and forwards the data to the cloud server via the Internet.
The doctor's PC-based application retrieves the data from the cloud server for analysis and visualization.

<br/>System-wide chain communication schematic diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/46.png" width="700"  height="350" />
<br/>ESP32C3 main control wiring diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/47.png" width="700"  height="350" />
