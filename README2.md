# Testing
 <br/><br/>
##  Test Bench Instruments
<br/>&emsp;&emsp;1.Life Signs Simulator (FLUKE Prosim 8p): To feed controlled ECG-like signals (1 mV amplitude with 1 Hz to 2 Hz frequency typical of heart signals, plus superimposed noise).
<br/>&emsp;&emsp;2.Digital Multimeter(FLUKE F18B): To measure battery voltage and confirm ADC readings.
<br/>&emsp;&emsp;3.Power Analyzer / DC Load(FLUKE NORMA 4000): Monitors current draw over time during idle, sampling, and BLE transmission states.
<br/>&emsp;&emsp;4.Oscilloscope: It can accurately detect and analyze relevant signals. At the same time, it can verify the ECG signal emitted by the vital signs simulator, and if it does not meet the conditions, the ECG signal will not be detected. At the same time, it can verify the ECG signal emitted by the vital signs simulator, and if it does not meet the conditions, it can be fine-tuned.
<br/>&emsp;&emsp;5.Signal Generator: simulate large voltage pulses and interference signals.
<br/>&emsp;&emsp;6.DC Adjustable Power Supply: stably outputs the required DC signal to simulate the battery voltage.
## Controlled Environment
 <br/>&emsp;&emsp;Simulated Patient Setup: replicate human activities such as walking, jogging, and sudden jerks to validate device stability under dynamic conditions.
 <br/>&emsp;&emsp;Electromagnetic Interference (EMI) Simulation: Nearby hospital equipment such as defibrillators and electrosurgical units were activated to create realistic EMI, testing the resilience of the device. Nearby hospital equipment such as defibrillators and electrosurgical units were activated to create realistic EMI, testing the resilience of the ECG signal acquisition and transmission systems.
<br/>&emsp;&emsp; Constant Temperature Chamber and Incubator: Provide a stable and controlled test environment to fully test the thermal safety of implants.
## Testing Procedures
### Evaluation of ECG Signal Accuracy
&emsp;&emsp;Purpose: To verify the accuracy and reliability of ECG signal acquisition in the absence of noise and adverse conditions.
<br/>&emsp;&emsp;Procedure:
<br/>&emsp;&emsp;1. HR test: The AD8232 ECG module was connected to a specially designed virtual tissue model with electrical and mechanical properties similar to human tissues, including a conductive gel layer to simulate skin impedance, and the accuracy test was performed by using a FLUKE ProSim 8P as a signal generator to simulate an HR of 40-180 BPM. Ethically approved live pigs were used for comparative validation at a later stage. Virtual models were calibrated using known impedance levels ranging from 500 to 1000 ohms.
<br/>&emsp;&emsp;2. Baseline testing: A known sinus rhythm ECG waveform (60 BPM, no noise) was generated and fed into the AD8232 input. The raw ADC readings on the ESP32C3 are recorded and the heart rate extraction algorithm is verified to produce ~60 BPM ± 1 BPM.
<br/>&emsp;&emsp;3. Noise immunity test: Introduced controlled electromagnetic interference from nearby equipment at 50 Hz and 60 Hz to simulate a hospital environment. Signal degradation was measured, including signal-to-noise ratio degradation and distortion indices. The noise suppression efficiency of real-time adaptive filtering techniques was applied and evaluated.
<br/>&emsp;&emsp;4. Sensitivity and thresholding: Threshold-based R-peak detection was adapted in firmware to avoid false detection.
<br/>&emsp;&emsp;5. Motion artifacts: Patient activities such as walking, sitting and standing, and sudden movements were simulated. Signal distortion was quantified by analyzing deviations in R-wave amplitude and evaluating the recovery rate of the artifact removal algorithm.
 <br/>&emsp;&emsp;This test could not be performed due to the lack of a vital signs simulator (FLUKE ProSim 8P), but the experimental procedure and methodology were fully feasible and I was able to operate the equipment proficiently.
### Pacing Pulse Generation Circuit Effectiveness and Safety
&emsp;&emsp;Purpose: Measure the output parameters when the pacing pulse is generated to evaluate its safety and effectiveness.
<br/>&emsp;&emsp;Procedure:
<br/>&emsp;&emsp;1. Simulation trigger: the ECG signal of sinus bradycardia was simulated using a vital signs simulator (FLUKE ProSim 8P), given to the electrodes, and an oscilloscope was used to monitor whether the electrodes output a pacing pulse.
<br/>&emsp;&emsp;2. Pacing Pulse Accuracy: Use an oscilloscope and multimeter to test the parameters related to the pacing pulse emitted, noting its pulse width, amplitude, frequency and current, and the deviation from the set value.
<br/>&emsp;&emsp;3. Large voltage blocking test: Inject large voltage pulses (500V) with the signal generator, the pulse width is 50us each time, and use the oscilloscope to detect whether the AD8232 unit and the pacing pulse generator unit successfully block the large voltage pulses. At the same time, the relevant waveforms were recorded with an oscilloscope to observe whether the blocking delay was within a reasonable range.
 <br/>&emsp;&emsp;Due to the lack of relevant experimental equipment such as the vital signs simulator (FLUKE ProSim 8P), this test could not be carried out, but the experimental steps and methods are completely feasible, and I can skillfully operate the relevant equipment.
### Wireless charging efficiency and battery performance
&emsp;&emsp;Purpose: To measure the power consumption during continuous operation and evaluate the efficiency of the wireless charging mechanism.
 <br/>&emsp;&emsp;Procedure:
 <br/>&emsp;&emsp;1. POWER ANALYSIS: A precision multimeter (FLUKE F18B) was used to monitor the currents and voltages in the active and idle states. The average current at different activity levels (e.g., continuous monitoring, idle, BLE transmission) was recorded.
 <br/>&emsp;&emsp;2. Charging efficiency test: the wireless charging transmitting and receiving coils were separated by different distances (10 mm to 50 mm) and misalignments (±30 mm), while the palm of the hand or pork could be used to simulate the isolation of the muscle tissue during implantation. Input and output power were measured using a power analyzer, and efficiency was calculated as the ratio of output power to input power.
 <br/>&emsp;&emsp;3. Long-term use simulation: switching between active and idle states over a 24-hour cycle to simulate typical patient use patterns. The capacity retention of the battery was analyzed over 100 simulated days of charge/discharge cycles.
 <br/>&emsp;&emsp;4. Violent testing: the battery was subjected to a short circuit test and an extreme overload charging test at high temperatures (>50°C) to see if any safety issues would arise.
 <br/>&emsp;&emsp; Results
  <br/>&emsp;&emsp;-Current is below 10mA in idle state, below 60mA in extreme state, and 20-35mA average in working state.
  <br/>&emsp;&emsp;-Wireless charging efficiency maintains reliability with slight misalignment.
 <br/>&emsp;&emsp;-Battery did not catch fire or explode when short-circuited or at high temperatures
 Suggestion for material selection.
### Battery voltage monitoring accuracy
&emsp;&emsp; Purpose: To measure the accuracy of the battery voltage divider detection circuit.
  <br/>&emsp;&emsp;Procedure:
 <br/>&emsp;&emsp;1. Static Measurement: Simulate the battery charging process by gradually increasing the voltage between 2.7 V and 4.2 V in 0.1 V increments using a DC adjustable power supply. The ADC reading is recorded each time and compared to a calibrated multimeter.
 <br/>&emsp;&emsp;2. Runtime Depletion Test: Connect a Li-Polymer battery to simulate an actual discharge from 4.2 V ~ 2.7 V. Data is continuously recorded to ensure proper tracking of the voltage profile
  <br/>&emsp;&emsp;Results
 <br/>&emsp;&emsp; Average deviation between microcontroller-measured voltage and multimeter reference is kept below 2-3%. Minor errors were corrected by calibration constants in the firmware.
 <br/>Diagram of a battery voltage log depicting a 6-hour battery discharge. The green dashed line is the multimeter reference measurement and the red solid line is the analog measurement captured by the ESP32C3. The analog data incorporates random noise to reflect possible measurement errors in real scenarios.<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/61.png" width="700"  height="350" />

### Implant Stability (Accelerometer) Tests
 <br/>&emsp;&emsp;1. Static Test: The MPU6050 was placed on a level surface. Data from the three axes (X, Y, Z) were recorded. Ideally, only one axis (Z) should show ~1 g while the others remain near 0 g. The MPU6050 was placed on a level surface. Ideally, only one axis (Z) should show ~1 g while the others remain near 0 g.
 <br/>&emsp;&emsp;2. Motion Simulation: The board was tilted at ~45° angles in different directions, and the readings were compared to expected sine/cosine projections of 1 g .
 <br/>&emsp;&emsp;3. Sudden Impact: The device was tapped or lightly shaken to see if large spikes could be recognized and whether the BLE link kept up with the sudden data changes.
 <br/>&emsp;&emsp;4. Long-term Drift: Over a 2-hour period, the accelerometer data were logged to see if any drift or sensor offset accumulation occurred.
### BLE Transmission Stability
&emsp;&emsp; Objective: To assess Bluetooth communication reliability in single-device and multi-device scenarios.
 <br/>&emsp;&emsp; Procedure: Range Testing: Measure packet success.
 <br/>&emsp;&emsp;1. Range Testing: Measure packet success rates and latency at distances ranging from 1 to 10 meters. Obstructions such as metallic barriers were introduced to simulate real-world interference. Obstructions such as metallic barriers were introduced to simulate real-world interference.
 <br/>&emsp;&emsp;2. Multi-Device Interference Testing: Perform normal transmission tests in an environment with more than 10 Bluetooth devices broadcasting to evaluate Monitor frequency conflict rates and implement adaptive frequency hopping to reduce interference.
 <br/>&emsp;&emsp;3. Latency Analysis: Measure time delays between data transmission and acknowledgment under varying device loads.
 <br/>Multiple tests on BLE via nRF connect <br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/62.png" width="350"  height="400" />
### Thermal Performance and Safety
&emsp;&emsp;  Goal: Ensure that the instrument maintains a safe temperature during prolonged use.
 <br/>&emsp;&emsp; Procedure:
1. Test Environment:
 <br/>&emsp;&emsp; The electrode temperature probe is placed in an ice-water mixture as well as in boiling water, and the collected external NTC temperature values are observed, and the acquisition program of the ESP32C3 needs to be calibrated if necessary.
 <br/>&emsp;&emsp; Testing was performed in a constant temperature chamber with an ambient temperature of 25°C. The implant prototype, including the electrode probes, was gathered and then powered up for acquisition and real-time monitoring via Bluetooth, while the coils were wirelessly charged.
 <br/>&emsp;&emsp; Temperature controlled thermostat with an ambient temperature of 37°C. The implant prototypes (including electrode probes) were placed in a virtual tissue model resembling human tissue (which can be replaced by pork), then powered up for acquisition and monitored in real time via Bluetooth, while the coil was charged wirelessly.
2. Temperature recording:
 <br/>&emsp;&emsp; Ambient room: an infrared camera with a resolution of 640x480 pixels and a sensitivity of 0.1°C was used to monitor the ambient room with the implanted device, with data recorded at 10-second intervals and collected over a 24-hour test period. The collected thermograms were analyzed to identify hot spots and assess the uniformity of heat dissipation on the device surface, as well as the temperature difference between operating and non-operating conditions.
 <br/>&emsp;&emsp; Thermostat: Logging via Android, data is recorded at 10-second intervals and temperature data received by Android is collected over a 24-hour test period.
3. Stress test:
<br/>&emsp;&emsp; All system functions (monitoring, communication and wireless charging) were enabled simultaneously to assess peak thermal loads. Data was captured using the same infrared camera and real-time thermal profiles were compared to baseline operating conditions to determine thermal stability. Heat accumulation rates and equilibration times were analyzed to understand thermal performance under extreme use.
## Notable Observations
ECG Noise Sensitivity:
<br/>&emsp;&emsp;The AD8232 module is sensitive to high-frequency interference, especially from switching regulators and high-frequency power transmission. Adequate shielding and short wiring helped reduce noise.
<br/>&emsp;&emsp;The wires of the ECG electrodes and the external NTC should be shielded signal wires, and the shielding layer should be grounded. At the same time, the circuit board should be shielded, and in order to reduce the electromagnetic radiation generated during wireless charging, the shielding cover should adopt a mesh structure.
<br/>Be careful to shield<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/63.png" width="700"  height="350" />
<br/>BLE Interference: 
<br/>&emsp;&emsp;In an environment with many overlapping Wi-Fi channels (2.4 GHz band), occasional increases in packet loss to ~3–5% were noted. Adjusting the BLE channel or employing adaptive frequency hopping improved reliability.
<br/>Calibration: 
<br/>&emsp;&emsp;Each sensor performed best after a one-time calibration step—particularly the ADC-based battery measurement. This step is recommended in any final product to account for resistor tolerances and ADC offsets.(At the same time, the calibration library inside ESP-IDF should be used, which uses the 1100mV reference voltage inside ESP.)
## Potential Improvements
Biocompatibility: 
<br/>&emsp;&emsp;Considering that WPT wireless power transmission technology will inevitably cause heating of metal parts, medical epoxy resin or medical silicone (metal is prohibited) should be used when selecting the shell package, and titanium or platinum should be selected for the electrode head. Due to the presence of a WPT receiving coil inside, more physical tests are needed to fully prove its safety (including the material of the electrode wire).
<br/>Titanium Alloy Electrode<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/64.png" width="700"  height="350" />
<br/>Thermal Safety:
<br/>&emsp;&emsp;The safety of lithium rechargeable batteries within implants, as well as the thermal effects caused by wireless charging, require more extensive testing to verify that they meet the safety standards for implanted medical devices.
<br/>Highly integrated circuit boards:
<br/>&emsp;&emsp;The hardware design in Chapter 3, except for the circuit board of the wireless charging module, has been fully integrated into a 4-layer PCB. In the case of the SD card data module, the chip-type SD NAND FLASH is used to further reduce the size. In the future, the wireless charging receiver circuit board can be completely polymerized together, so that there is only one circuit board inside the implant. The thickness of the implant can be greatly reduced, and the overall size can be controlled within 35mmx35mmx15mm. However, due to time constraints as graduation approached, this step was not realized.
<br/> Package shaping:
<br/>&emsp;&emsp; due to the need for wireless charging, this implant cannot use a metal shell; the shell should be shaped by 3D printing using medical polymer materials. The polymerized main board, wireless charging receiving circuit board and receiving coil, battery, in the order of the outermost coil-battery-wireless charging circuit board-main board, are put into the medical polymer shell, and then potted with medical silicone, while the gap at the electrode lead-in position is also sealed with silicone.
<br/>Medical-grade potting silicone<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/65.png" width="700"  height="350" />
