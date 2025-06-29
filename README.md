# Smart-Implant-Project
A prototype of a wired smart cardiac pacemaker with continuous monitoring functionality, featuring low-power Bluetooth communication and wireless charging capabilities.
## The overall objective of this intelligent cardiac pacemaker system is:
1.Real-time, high-precision, and highly sensitive monitoring of the patient's ECG, motion inertial status, internal temperature of the implantable device, external electrode temperature, and battery voltage.<br/>
2.Upon detecting bradycardia, the device delivers a pacing pulse current within an extremely short timeframe to initiate pacing.<br/>
3.All monitoring data is stored in real time in the storage unit. The implantable device's storage capacity can store at least 15 days of monitoring data.<br/>
4.Can communicate with the patient's mobile app via low-power Bluetooth (BLE) to transmit real-time and historical data to the patient's mobile device. Patients can view real-time and historical ECG waveforms and intelligent analysis through the mobile app, as well as check implant-related alerts and abnormal notifications.<br/>
5.Historical ECG data and device logs can be uploaded to the cloud via the mobile app. The cloud stores historical data and performs intelligent analysis. Medical personnel can access the cloud service to view patients' historical ECG data, device anomaly data, and intelligent analysis results.<br/>
6.Wireless charging is supported, with a single charge providing over 7 days of battery life. When the wireless charging transmitter is in operation, it must be connected to the mobile app via a data cable to initiate charging. If the implant temperature becomes abnormal, the transmitter will automatically shut down.<br/>
7.The overall dimensions of the implant should be less than 40mm x 40mm x 15mm, with excellent safety and biocompatibility.<br/>
## To achieve the overall objective, the architecture of this cardiac pacemaker system includes:
l Sensors: ECG module, battery voltage divider, accelerometer, external and internal temperature sensors.<br/>
l Microcontroller: MCU acquires sensor data, processes signals, and handles wireless communication (BLE).<br/>
l External storage unit: MicroSD card mounting circuit.<br/>
l Enhanced antenna: Enhances BLE signal strength, increases communication range, and ensures stable data transmission to PC/phone devices.<br/>
l Pacing pulse generator circuit: Based on the ECG signals received, it outputs constant current pulses in an extremely short time for cardiac pacing.<br/>
l Pacing pulse safety management circuit: Ensures the safety and stability of the pacing pulse-related circuits.<br/>
l Power reception regulation module: Responsible for regulating the received wireless power through rectification, filtering, and voltage regulation, and charging the battery.<br/>
l Receiving coil: Receives wireless power from an external transmitter.<br/>
l Battery pack: Stores electrical energy received from the power regulation module and provides power when not charging.<br/>
l Transmitting power regulation module: Converts the constant voltage source current into high-frequency resonance, communicates with the patient's mobile app via a serial port, and controls power transmission.<br/>
l Transmitting coil: Transmits modulated wireless electrical energy.<br/>
l User device: An Android application that displays real-time data, stores logs, and issues alerts.<br/>
l Cloud service: Stores patient-related data, performs cloud computing and related intelligent analysis. Medical personnel access the cloud service for follow-up and manual analysis.<br/>
<br/>Signal measurement and wireless transmission diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/111.png" width="700"  height="350" />
<br/>Schematic diagram of the smart pacemaker<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/333.png" width="700"  height="350" />
<br/>Schematic diagram of the internal structure of the implant<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/222.png" width="700"  height="350" />
## Main Components of the Hardware Module
l ECG32C3 Microcontroller Main Control Module <br/>
l AD8232 ECG Detection Module<br/>
l MPU6050 Inertial Motion State Detection Module<br/>
l SD Card External Storage Module<br/>
l NTC External Temperature Detection Module<br/>
l Battery Voltage Detection Module<br/>
l Wireless Charging and Control Module<br/>
### Design of the ECG32C3 microcontroller master module
<br/>Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/39.png" width="700"  height="350" />
<br/>PCB finished drawing<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/31.png" width="700"  height="350" />
<br/>PCB 3D<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/30.png" width="700"  height="350" />
### AD8232 ECG Detection Module Design
<br/>Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/42.png" width="700"  height="350" />
<br/>PCB finished drawing<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/29.png" width="700"  height="350" />
<br/>PCB 3D<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/28.png" width="700"  height="350" />
### MPU6050 Inertial Motion State Detection Module Design
<br/>Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/44.png" width="700"  height="350" />
<br/>PCB finished drawing<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/27.png" width="700"  height="350" />
<br/>PCB 3D<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/26.png" width="700"  height="350" />
### SD card external memory module design
Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/45.png" width="700"  height="350" />
<br/>PCB finished drawing<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/25.png" width="700"  height="200" />
<br/>PCB 3D<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/24.png" width="700"  height="350" />
### NTC External Temperature Detection Module Design
<br/>Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/23.png" width="700"  height="350" />
### Battery Voltage Detection Module Design
<br/>Complete Circuit Schematic<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/22.png" width="700"  height="350" />
### Testing of circuit boards for various hardware modules
Write programs to run on the ESP and perform functional verification tests on each pin module.
<br/>AD8232 test result output<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/33.JPG" width="700"  height="350" />
<br/>MPU6050 test result output<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/20.png" width="700"  height="350" />
<br/>SDcard External Storage Module Test Wiring Diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/19.png" width="700"  height="350" />
<br/>SDcard external storage module test result diagram A<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/18.png" width="700"  height="350" />
<br/>SDcard External Storage Module Test Results Diagram B<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/17.png" width="700"  height="350" />
<br/>All module aggregation test results<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/34.JPG" width="700"  height="350" />
### Wireless charging module
This paper analyses the calculation principles of key parameters related to wireless charging, including topological structures, inverter modes, coils, and resonant frequencies, and conducts simulation tests using MATLAB Simulink. However, due to the war, it is difficult to purchase materials, and no physical tests have been conducted yet.
<br/>LCC-S compensation topology<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/1.png" width="700"  height="300" />
<br/>Wireless charging Matlab Simulink simulation test<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/2.png" width="700"  height="300" />
### Wireless charging control design
&emsp;&emsp;Due to the possible temperature rise caused by wireless charging, the temperature of the implant must be strictly monitored and controlled to ensure safety.<br/>
&emsp;&emsp;One temperature sensor (within MPU6050) and the NTC of the external electrode on board of the main control of the implant, a total of 3 sets of temperature values are collected by the main control in real time and can be transmitted to the patient's cell phone terminal. Therefore, during wireless charging, it must be controlled through the cell phone terminal, and the charging transmitter is allowed to operate only when all 3 sets of temperatures are within the safe range value (40°C).<br/>
&emsp;&emsp;To do this, a relay can be designed to turn on and off the 12V DC input of the transmitter, and the relay signal is controlled by the signal output from another ESP32C3 development board, which interacts with the cell phone APP through the serial port via the USB cable.<br/>
&emsp;&emsp;This design can realize the wireless charging when the three sets of temperature values are normal, during the process of the cell phone app to monitor the temperature is abnormal, immediately through the serial port to send commands to stop charging.<br/>
<br/>Wireless charging control circuit schematic diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/38.png" width="700"  height="350" />
<br/>Wireless charging control circuit PCB design (front)<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/4.png" width="700"  height="350" />
<br/>Wireless Charging Control Circuit PCB Design Diagram (Reverse)<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/5.png" width="700"  height="350" />
<br/>Wireless charging control circuit board 3D drawing<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/6.png" width="700"  height="350" />
<br/>Wireless charging control schematic diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/15.png" width="700"  height="350" />
### Pacing pulse constant current circuit module
 &emsp;&emsp;This module is an experimental exploration of a simple pulse constant current generation circuit, based on the above hardware, add a control circuit and constant current generation circuit, through the intelligent master control to detect sinus bradycardia, to real-time output of a typical pulse constant current, and strictly control the pulse time of microseconds.<br/>
 &emsp;&emsp;This project only explores a kind of experimental pacing pulse constant current generation design, the relevant parameters are in line with the characteristics of pacing current, but to be used in human beings, its safety and so on need to be explored rigorously.<br/>
 <br/>AD8232 ECG Detection Module Improvement Diagram<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/43.png" width="700"  height="350" />
 <br/>Pulse Pacing Safety Circuit A<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/40.png" width="700"  height="350" />
<br/>Pulse Pacing Safety Circuit B<br/>
  <img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/41.png" width="700"  height="350" />
 &emsp;&emsp;The AD8232 ECG detection circuit and pacemaker pulse generator circuit have been improved and integrated, while strictly adhering to safety requirements, and the following safety functions have been achieved:
1. Leakage current at both ends of the electrode is almost 0, much less than the international standard of 10uA. and the electrode has been kept connected under normal conditions, normal overcurrent ECG signal, no distortion and no significant attenuation. The selection of all components strictly consider the overcurrent and voltage, and try to miniaturize (0402), using high precision (1%), the board size increase is not obvious.<br/>
2. When sinus bradycardia is detected, the pacing pulse generator circuit outputs a constant current pulse of 5mA. The pacing pulse circuit chip is overvoltage protected and self clamps when it exceeds 5V. When there is an extreme internal failure, when the voltage is greater than 7.2V, the pacing generator circuit TVS will break down, blowing the disposable SMD fuse, and there will be no more output from the pulse constant current circuit.<br/>
3. The AD8232 cardiac detection circuit can safely clamp large voltage pulses of up to 700V at the electrodes.<br/>
4. The large voltage withstand diode at the LA electrode of the pacing pulse circuit can effectively block the external large voltage pulse.<br/>
5. The RA electrode of the pacing pulse circuit has two voltage divider circuits and a high-speed comparator, so that when a potential of 6V is detected at the LA electrode, the path from the LA electrode to the pacing pulse circuit is immediately cut off by a MOS tube.<br/>
 &emsp;&emsp;A comprehensive analysis and effective estimation of the action delay and shutdown delay of the pacemaker pulse generator circuit were performed. The delays are fully compliant with the system design objectives and have minimal impact on related functions. Subsequent testing can be conducted by simulating pacemaker pulses and performing animal experiments, using an oscilloscope to test the electrocardiogram signals and safety, including response time.<br/>
### Hardware Module Fusion Design
   &emsp;&emsp;The first thing that can be determined is that the following modules can be well integrated together:
 ESP32C3 master control circuit module<br/>
 AD8232 ECG signal detection circuit module<br/>
 MPU6050 inertial motion detection circuit module<br/>
 NTC external temperature detection circuit module<br/>
 Battery voltage detection circuit module<br/>
 Data storage module<br/>

 <br/>Improvements to SDNAND storage module circuits<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/37.png" width="700"  height="350" />
<br/> Lithium battery protection circuit<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/36.png" width="700"  height="350" />
<br/> Lithium Battery Protection Circuit PCB Front Side<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/14.png" width="700"  height="350" />
 <br/>Lithium Battery Protection Circuit PCB Rear Side<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/13.png" width="700"  height="350" />
<br/> Lithium battery protection circuit PCB 3D<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/12.png" width="700"  height="350" />
  &emsp;&emsp;<br/>The implant aggregation main board should include the following hardware modules:<br/>
 ESP32C3 main control circuit<br/>
 AD8232 ECG signal detection circuitry<br/>
 MPU6050 motion inertia state circuit<br/>
 External NTC detection circuit<br/>
 Battery voltage detection circuit<br/>
 SDNand storage circuit<br/>
 Pacing Pulse Generation Circuit<br/>
 Pacing pulse safety control circuit<br/>
 And related interfaces and pads<br/>
<br/> The implant polymerization motherboard, whose circuit diagram is analyzed above, is drawn and completed as follows:

<br/>  Circuit diagram of the implant polymerization motherboard<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/35.png" width="700"  height="350" />
  <br/>Implant Aggregation Motherboard PCB Top<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/11.png" width="600"  height="400" />
  <br/>Implant Polymerization Motherboard PCB Inside A<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/9.png" width="600"  height="400" />
 <br/> Implant Polymerization Motherboard PCB Inner Layer B<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/10.png" width="600"  height="400" />
 <br/> Implant Polymerization Motherboard PCB Bottom<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/8.png" width="600"  height="400" />
  <br/>3D image of finished implant PCB circuit board（The pin headers in the picture represent the wiring,here are no actual pin headers.）<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/7.png" width="600"  height="200" />
   <br/> &emsp;&emsp;This polymerization motherboard PCB is conceived and designed in full accordance with the above analysis, and it fully meets the due standards and requirements, and passes the automatic DRC test according to the industry default standard.<br/>
   &emsp;&emsp;And in the top layer of the PCB, I have reserved a blank space for the wireless charging receiver circuit board, currently close to graduation, time is limited, will be in the next few months, the wireless charging receiver circuit board is also integrated in the implant polymerization board, so that the implant only consists of the polymerization board, the battery, the receiver coil, the implant can be controlled within the overall thickness of 15mm.<br/>
   &emsp;&emsp;In the paper, we conducted extensive analysis and necessary calculations on all key details of the circuit design and component selection. Due to space limitations, we will not list them all here.<br/>
<br/>
#### Please refer to README1.md/README2.md for the software code and debugging section.
