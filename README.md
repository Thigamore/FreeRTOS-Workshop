# FreeRTOS-Workshop

## Intro

This is the code for the FreeRTOS Workshop led by IEEE on 11/26/2026.

The code is split up into 5 main parts.
1. Starter: The starter code with all the helper functions built in to access the leds, buttons, and game logic.
2. Final: The final code after all the parts are done that can be used as a reference.
3. Part 1: Sample code after part 1 that makes use of tasks and scheduling.
4. Part 2: Sample code after part 2 that makes use of mutexes for data passing.
5. Part 3: Sample code after part 3 that utilizes notifications to communicate between tasks.

All the code is meant for an ESP32-S3 and has not been tested on other systems. Pins may have to be changed if a different ESP is used but otherwise the code should be fine.

## Installation of ESP-IDF

There are 2 main ways:
1. Through the vscode extension which will be covered below.
2. Through a CLI covered [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

### VS Code ESP-IDF Installation

1. Go to vscode and the extension tab (Ctrl + Shift + x or Cmd + Shift + x).
2. Download the ESP-IDF extension by Espressif Systems.
3. Go to the ESP-IDF: Explorer extension in the side bar. (If you cannot see this then you can right click the bar and enable it or do all the commands through ctrl + shift + p or cmd + shift + p)
4. Click on the advanced drop down and hit the "Open ESP-IDF Installation Manager". This will download the installation manager to download the esp idf tool.
5. Next a GUI will appear after it is done downloading. Hit the Easy Installation button and wait for it to install. This may take a while as it is 3.5GB.
6. Restart your VSCode to ensure the changes are propogated.

## Building and Flashing using ESP-IDF

Before building, note that the folder you should be in should be the directory containing main. If it is in the top level directory, then there might be issues finding the right folder

### With VSCode

1. Set the target using "Set Espressif Device Target".
2. Choose your target as ESP32-S3 and choose the first option for the flashing method.
3. Hit the "Build Project" button and watch the project build. If this infinite loops, then stop in and retry it.
4. Hit the Flash Device button. If it asks you how to flash it, use Flash with UART.

### With the CLI
1. Open up the cli through EIM shell.
2. Do ``` idf.py set-target esp32s3 ``` to set the target to our esp32s3.
3. Do ``` idf.py build ``` to build.
4. Do ``` idf.py flash ``` to flash. You might have to choose a port. If so, you can choose the port through ``` idf.py flash --port PORT ``` by replacing PORT with your port.