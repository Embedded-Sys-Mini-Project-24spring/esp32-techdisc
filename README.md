# esp32-techdisc

Kangcheng Chen  
Noah Mecham  
Carly Atwell

## Purpose
This project allows ultimate frisbee players or disc golfers to improve their throws by tracking the rotational speed and angle of the disc as they throw it. The hardware attaches to the bottom of a disc and streams data about the disc's flight over wifi to a web app where we can view the information in real time. Rotational speed and the angle of the disc are impprtant factors in increasing the distance or hang time of a throw, and also important in controlling a disc in the wind, so this tool provides valuable data to anyone looking to improve their disc chucking abilities. 

## Setup 

### Hardware

- ESP32-S2-Saola-1
- MPU6050 
- MicroUSB cable and female-to-female jumper wires

![techdisc (1)](https://github.com/Embedded-Sys-Mini-Project-24spring/esp32-techdisc/assets/67492291/6d082b46-6f90-41d1-a87c-8cbe24ae6b3b)

### Software

You can use [PlatformIO](https://platformio.org/) or [ESP-IDF](https://idf.espressif.com/).

If you are using VScode, you can install either of them as a plugin.

- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)
- [PlatformIO ESP32 Guide](https://docs.platformio.org/en/stable/core/quickstart.html#process-project)

## Frontend 

The client side app [tech-disc-app](https://github.com/Embedded-Sys-Mini-Project-24spring/tech-disc-app) is an Typescript application running in the browser. It is deployed on [Github pages](https://embedded-sys-mini-project-24spring.github.io/tech-disc-app/) and openly accessible. 

## Data processing

## Communication

Reference:

1. [MPU-6050-Register-Map1.pdf](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)
