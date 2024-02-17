# SETUP TIPS

## CONFIGURE VSCode for ESP 
- install c/c++ extension
- ctrl shift p, c/c++ configurations
- scroll to advanced, compile commands
- set as ${workspaceFolder}/build/compile_commands.json

## ESP32 devkit v1
- [pinout reference](https://lastminuteengineers.com/esp32-pinout-reference/)
- GPIO 2 is onboard LED
- GPIO 0 is boot button

## STARTING ESP32 FreeRTOS Project
- open CMakeLists.txt, change project name in line 6
- idf.py set-target esp32
- idf.py menuconfig
- idf.py build

## FLASHING AND MONITORING ESP32
- check port by opening device manager
- idf.py -p COMX flash
- must hold boot button on "Connecting" prompt
- idf.py -p COMX monitor
- exit with "ctrl+\[" or "ctrl+ç"

## ENABLING ESP32 FreeRTOS Trace
- idf.py menuconfig
- Component config > FreeRTOS
- check Enable FreeRTOS trace facility
- check Enable FreeRTOS to collect run time stats
- shift S, enter, shift Q
- make a task containing the following lines:
```
  char msg[2048];
  vTaskList(msg);
  printf("\r\nTask---------State---Prio------Stack---Num");
  printf("\r\n%s", msg); // List of tasks and states
  printf("\r\n------------------------------------------");
  vTaskGetRunTimeStats(msg);
  printf("\r\n%s", msg); // Time usage percentages
```

## KNOWN PROBLEMS
- The installation path of ESP-IDF and ESP-IDF Tools must not be longer than 90 characters
- Infinite loop on re-running cmake: check creation dates on files (cannot be in the future)
