 # software code and debugging
 <br/>This project involves the relevant programs for the ESP32C3 main controller, based on the ESP-IDF development environment, version 5.2.X，with C/C++ as the programming languages.<br/>
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
### ESP32 Program Development
<br/>The ESP32 program is located in folder ESP32C3_MAIN, and the relevant code is still being refined.<br/>
<br/>Program development principle:<br/>
 <br/>&emsp;&emsp;This program development is carried out by modular code, ESP-IDF development is based on C/C++ programming, we can write the configuration code of the related modules in special .c and .h files, which is convenient for the main.c file to call. Pay attention to three development principles:
<br/>&emsp;&emsp;1.The code in development should be concise and efficient to avoid the waste of computing resources.
<br/>&emsp;&emsp;2.Modular design: each function is encapsulated in a separate module for subsequent modification and expansion.
<br/>&emsp;&emsp;3.Extensibility: Interfaces are reserved for future module functions.<br/>
<br/>Program workflow:<br/>
<br/>1.Initialize all hardware peripherals after system startup
<br/>2.Creates multiple independent tasks to run in parallel)<br/>
&emsp;&emsp; ECG signal acquisition and processing (ADC)<br/>
&emsp;&emsp; Motion status monitoring (I2C)<br/>
&emsp;&emsp; Temperature monitoring (ADC)<br/>
&emsp;&emsp; Battery voltage monitoring (ADC)<br/>
&emsp;&emsp; Data storage (SPI)<br/>
&emsp;&emsp; Low-power Bluetooth broadcasting and communication<br/>
<br/>3.Precise sampling control by timer
<br/>4.Because of the use of multiple ADC analog signal acquisition at high speed, should be rigorously set up mutual exclusion locks, to avoid the acquisition of the timeout and program crash
<br/>5.Buffer the collected data and write them to SD card in batch.
<br/>6. Monitor the system status in real time and record and handle abnormal situations.
<br/> 7.After receiving the external Bluetooth connection request, establish the connection, acquire the real time of the outside world and process the relevant commands sent by the outside world, including real-time monitoring and historical data synchronization tasks.<br/>
<br/>Specific module functions and design requirements are as follows:<br/>
<br/>1. Config.h - global configuration header file
<br/>&emsp;&emsp;Defines all configurable parameters
<br/>&emsp;&emsp;Contains sample rate settings
<br/>&emsp;&emsp;Define hardware pin mapping
<br/>&emsp;&emsp;Configure sensor parameters
<br/>&emsp;&emsp;Sets system thresholds and limits
<br/>2. adc_module--AD8232 ECG analog signal acquisition module
<br/>&emsp;&emsp;Responsible for AD8232 analog signal acquisition, set sample rate to 500hz (or 250Hz)
<br/>&emsp;&emsp;Configures ADC channels, accuracy and calibration
<br/>&emsp;&emsp;Includes error handling and timeout protection
<br/>3. hr_calculation--AD8232 heart rate calculation processing module
<br/>&emsp;&emsp;Real-time processing of ECG signals
<br/>&emsp;&emsp;Implement R-wave detection algorithm software filtering algorithm
<br/>&emsp;&emsp;Waveform data buffer management
<br/>&emsp;&emsp;Calculation of heart rate (BPM) 
<br/>4. pin_control--Cardiac lead shedding detection module
<br/>&emsp;&emsp;Initialize and manage GPIO pin configuration and control, set sampling rate to 1hz
<br/>&emsp;&emsp;Handles lead detection and AD8232 operating mode control
<br/>&emsp;&emsp;Manage AD8232 lead off detection.
<br/>&emsp;&emsp;Realize hardware reset function
<br/>&emsp;&emsp;Support external interrupt configuration
<br/>5. mpu6050--Inertial state acquisition and processing module
<br/>&emsp;&emsp;Realize I2C communication and sensor configuration, set the sampling rate to 25hz.
<br/>&emsp;&emsp;Real-time acquisition of accelerometer and gyroscope data, the realization of motion detection algorithms 
<br/>&emsp;&emsp;Support accelerometer, gyroscope and temperature data reading.
<br/>&emsp;&emsp;Provide motion amplitude calculation, set the patient sleep and wake-up determination algorithm, support sleep/wake-up state detection, turn off the gyroscope during sleep, and reduce the accelerometer sampling rate to 5hz.
<br/>6. ntc_sensor--External NTC acquisition and processing module
<br/>&emsp;&emsp;High resolution acquisition of external NTC voltage divider resistor, convert resistance value to temperature value.
<br/>&emsp;&emsp;Set sampling rate to 5hz and set software filtering algorithm
<br/>&emsp;&emsp;Realize temperature calibration and support multi-channel temperature measurement. 
<br/>7. battery_monitor--Battery Voltage Acquisition and Processing Module
<br/>&emsp;&emsp;Accurately measure the voltage of 3.3-4.3V lithium battery in real time, and set the sampling rate to 0.2hz.
<br/>&emsp;&emsp;Automatic low voltage alarm
<br/>&emsp;&emsp;Estimation of battery power by voltage setting algorithm.
<br/>&emsp;&emsp;Smooths voltage readings using a moving average filter. 
<br/>&emsp;&emsp;Supports battery voltage calibration
<br/>8. sd_card - data storage module
<br/>&emsp;&emsp;Automatically mounts the FAT file system during boot-up initialization.
<br/>&emsp;&emsp;Failed to mount always retry and print system level error, support formatting SD card (failed to mount)
<br/>&emsp;&emsp;Provide read/write API
<br/>&emsp;&emsp;Set up a ring buffer in the memory, and then write to the SD card after every 5 seconds.
<br/>&emsp;&emsp;The data directory is divided into 5 folders, storing ECG data/lead shedding data/acceleration gyroscope data/3 temperature sensors data/voltage and power data respectively, each file only stores data corresponding to one hour, with the file name of the corresponding date and hour, the data in the corresponding folder is saved in CSV file format, and the data are timestamps and values, the timestamps are based on the year-month-day-hour-minute-second-second. milliseconds.
<br/>9. oled_display - 0.96-inch OLED display (test module)
<br/>&emsp;&emsp;Manages ECG waveform and heart rate value displays
<br/>&emsp;&emsp;Provides interface for status and message display
<br/>10. ssd1306--SSD1306 driver (OLED)
<br/>&emsp;&emsp;Provides 0.96-inch OLED display driver
<br/>&emsp;&emsp;Support text and graphic drawing function
<br/>11. ble_module--Bluetooth communication module
<br/>&emsp;&emsp;Initialize Bluetooth and broadcast
<br/>&emsp;&emsp;Establish Bluetooth communication task after receiving external Bluetooth request
<br/>&emsp;&emsp;Synchronize external time to internal FreeRtos, process external real-time monitoring and historical data generation commands
<br/>&emsp;&emsp;After receiving real-time monitoring commands from the outside world, send the collected data via Bluetooth in real time at a set frequency.
<br/>&emsp;&emsp;After receiving the external historical data synchronization command, send the data files stored in the SD card to the external Android app via Bluetooth in real time.
<br/>12. Main.c--Main program file
<br/>&emsp;&emsp;Coordinate the work of each module
<br/>&emsp;&emsp;Create tasks
<br/>&emsp;&emsp;Configure timer to realize accurate sampling
<br/>SDcard Write effect diagram<br/>

<br/>Bluetooth time synchronization effect<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/49.png" width="700"  height="350" />
<br/>The effect of the aggregate testing A of each software module<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/56.png" width="700"  height="350" />
<br/>The effect of the aggregate testing B of each software module<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/57.JPG" width="700"  height="350" />
<br/>The effect of the aggregate testing C of each software module<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/58.JPG" width="700"  height="350" />
### Android application Program Development

 <br/>The main program files are as follows:<br/>
 <br/>MainActivity.java - the main program file, which handles BLE communication and main logic
 <br/>EcgActivity.java - responsible for processing and visualizing the received ECG data
 <br/>OtherSensorsData.java - conversion processing and visualization of data other than ECGs
 <br/>FileLogger.java - logging errors and exceptions to a txt file for debugging purposes.
 <br/>WirelessChargingController.java--Wireless charging control module, sends commands to USB serial port through temperature monitoring.
 <br/>DataCloudService.java - historical data acquisition and upload module

  <br/>Android & ESP32C3 BLE linkage test <br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/50.png" width="700"  height="350" />
 <br/>BLE test chart on Android side<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/55.png" width="700"  height="350" />
 <br/>Real-time monitoring test on Android side<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/51.png" width="300"  height="450" />
 <br/>Logging test on the Android side<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/52.png" width="700"  height="350" />
### Cloud Program Development
<br/>&emsp;&emsp;1.The cloud infrastructure should come with a database to write the data uploaded from the Android side to the database for use.
<br/>&emsp;&emsp;2.It should be able to provide UI web pages for external access and visualization and intelligent analysis of physiological data.
<br/>&emsp;&emsp;3.There is a user login interface, ECG waveforms and other visualization UI interface design should be simple and efficient.
<br/>The cloud code is still being improved and will be updated in the future.
<br/>Cloud UI Effect A<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/53.png" width="700"  height="350" />
<br/>Cloud UI Effect B<br/>
<img src="https://raw.githubusercontent.com/niceguyhere/implant-project/refs/heads/master/image/54.png" width="700"  height="350" />
