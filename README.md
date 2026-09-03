# ESP32-MAX30102-PPG-SpO2-Heart-Rate-measure-display
Real time PPG, heart rate (BPM), and blood oxygen saturation (SpO₂) monitoring using ESP32, MAX30102, and 1.3″ IIC OLED display.
<img width="600" height="400" alt="3VVIN" src="https://github.com/user-attachments/assets/4b4f05f3-a0de-499e-ab70-2a77715aaf59" />

<img width="600" height="400" alt="esp32" src="https://github.com/user-attachments/assets/6f8af6de-cea8-434a-9a6f-6cc896ab4d97" />

## Components
ESP32 S3 WROMM 1/ESP WROOM 32, SH1106 (128 × 64) OLED I2C(IIC) Display, MAX30102 Sensor, Jumper Wire, usb type c/micro usb cable

## Libraries
SparkFun MAX3010x Pulse and Proximity Sensor Library
Adafruit GFX Library
Adafruit SH110X

## Procedure
Connect ESP32 with the other components according to the given schematic then connect the ESP32 to laptop/PC using appropriate USB cable.
Download Arduino IDE from here: https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE and install it. 
Open the Arduino IDE, create a new sketch, and paste the necessary code in there.
From top in Arduino IDE user interface  select the port-board/module you're using.


<img width="443" height="200" alt="image" src="https://github.com/user-attachments/assets/3d042d60-70f3-4c62-b714-73456d02e97b" />


If the board is properly selected, at the very bottom of the user interface it'll show something like this,


<img width="277" height="116" alt="image" src="https://github.com/user-attachments/assets/da94e333-4f6d-4c66-b560-67cb982a5a48" />


Click Verify to compile the code. Once verification is successful, click Upload to flash the code to the ESP32. Depending on the ESP32 board, you may need to press and hold the BOOT button while the upload process begins. Release it once the IDE starts uploading the code.

After the upload is complete, open Tools → Serial Monitor.
Set the baud rate to 115200.

The program output should now appear in both the Serial Monitor and the OLED display.
