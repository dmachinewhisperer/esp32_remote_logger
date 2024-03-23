# Introduction

PID controllers are widely used in robotics and process automation. Often designers recompile the executable many times when tuning the controller. This component provides a window into the controller to tune it in real time, avoiding hardcoding the gain values and therefore multiple recompilations of the source files. 

# Target Framework and Device
ESP-IDF and ESP32 family of microchips (written entirely in C and  easily adaptable to work on other platforms)

# Tuning Strategies Supported
## Local Tuning
- You need to build a simple circuit as described below (default pins).
```
    a. POT1    -> ADC1_CHAN0
    b. POT1    -> ADC1_CHAN3
    c. POT1    -> ADC1_CHAN6
    d. tactile -> GPIO13
```
- You may set the pins as you wish in the Kconfig file at the root of the component. 

- When the tactile is pressed, an interrupt fires and the ISR samples the ADCs which the POTs are connected to. These values are assigned to P, I, D global variables and written to NVS to persist between power cycles. 

## Network Tuning. 
- You need to setup a simple app (such as the one provided alongside). 
- When the components is configured to enable this mode, it opens a WebSocket server on port 80. You need to send the PID values in the form "P I D" over this connection to the server. The values are modified and written to NVS. 

# Setup 
Include the component in your project. 
The following boilerplate must be present in your application code to configure the interfaces properly. 
```
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
```
 - To load previous PID values, before starting any type of tuning, load the values from NVS:
 ```
     load_pid_values();
 ```
## If using Local Tuning
- In your application code, call:
```
    start_local_pid_tuner();
```
- Build your circut and modify the Kconfig if need be (i.e if using different pins)

## If using Network Tuning
- In your application code, call, 
```
    start_socket_pid_tuner();
```
- Enable WebSocket over HTTP in menuconfig, 
```
menuconfig -> Component Config -> HTTP Server -> Websocket Server Support
```
![alt text](image.png)

- Set connection parameters 
```
menuconfig -> Example Configuration
```
![alt text](image-1.png)
 

# Running

- If using network tuning, when the application starts up, the IP of the server is printed on the serial console. Feed this to your frontend application and send the appropirate string format ("P_value I_value D_value")

- If using the provided basic webapp, set the following line accordingly in `script.js` :
```
ws = new WebSocket("ws://172.22.202.69:80/ws");
``` 

# Notes
- The two tuning methods can be used independently or at the same time without conflicts.
- The ADC attenuation is set to ADC_ATTEN_DB_12 limiting the measurable input range to 150 mV ~ 2450 mV. Plan accordingly when setting up the POTs.

# Sample Serial Monitor Output
```
I (813) example_connect: Connecting to GITAM...
I (813) example_connect: Waiting for IP(s)
I (3223) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1

I (5603) esp_netif_handlers: example_netif_sta ip: 172.22.202.69, mask: 255.255.240.0, gw: 172.22.192.1
I (5603) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 172.22.202.69
I (5613) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:b2b2:1cff:fea7:4080, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5623) example_common: Connected to example_netif_sta
I (5633) example_common: - IPv4 address: 172.22.202.69,
I (5633) example_common: - IPv6 address: fe80:0000:0000:0000:b2b2:1cff:fea7:4080, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (5643) ws_server: Starting server on port: '80'
I (5653) ws_server: Registering URI handlers
I (5653) main_task: Returned from app_main()

I (42473) ws_server: Handshake done, the new connection was opened
W (42493) wifi:[ADDBA]rx delba, code:37, delete tid:0
I (46783) ws_server: frame len is 8
I (46783) ws_server: Got packet with message: 65 36 50
I (46783) ws_server: Packet type: 1
I (46783) ws_server: P, I, D was set Remotely.
I (46803) helper: PID saved to NVS: P = 65, I = 36, D = 50

I (6366) pid_tuner_local: P = 951, I = 0, D = 0
I (6376) helper: PID saved to NVS: P = 951, I = 0, D = 0
I (14446) pid_tuner_local: P = 952, I = 532, D = 0
I (14456) helper: PID saved to NVS: P = 952, I = 532, D = 0
```