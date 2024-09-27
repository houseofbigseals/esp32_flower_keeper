

# Flower Keeper 
Simple automation system for watering flower based on esp32 chip. Relay will be triggered by timer, and also can be remote controlled via http server over wireguard.  
Server created from default esp-idf example () and wireguard implementation stolen from [trombik`s lib](https://github.com/trombik/esp_wireguard)

IMPORTANT NOTE:  
You need to set CONFIG_LWIP_PPP_SUPPORT in idf.py menuconfig, it will fix the problem with crash wg lib 
[see that comment](https://github.com/trombik/esp_wireguard/issues/33#issuecomment-1568503651)
Also, "Peer is down" state can persist up even one or two minutes, so wait patiently

## How to use 


### Hardware description
Relay - gpio 17  


