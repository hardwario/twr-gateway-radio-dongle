<a href="https://www.bigclown.com/"><img src="https://bigclown.sirv.com/logo.png" width="200" height="59" alt="BigClown Logo" align="right"></a>

# Firmware for BigClown USB Gateway

[![Travis](https://img.shields.io/travis/bigclownlabs/bcf-usb-gateway/master.svg)](https://travis-ci.org/bigclownlabs/bcf-usb-gateway)
[![Release](https://img.shields.io/github/release/bigclownlabs/bcf-usb-gateway.svg)](https://github.com/bigclownlabs/bcf-usb-gateway/releases)
[![License](https://img.shields.io/github/license/bigclownlabs/bcf-usb-gateway.svg)](https://github.com/bigclownlabs/bcf-usb-gateway/blob/master/LICENSE)
[![Twitter](https://img.shields.io/twitter/follow/BigClownLabs.svg?style=social&label=Follow)](https://twitter.com/BigClownLabs)

This repository contains firmware for BigClown USB Gateway.

## Firmware Programming
```
dfu-util -s 0x08000000:leave -d 0483:df11 -a 0 -D firmware.bin
```
More information about dfu [here](https://doc.bigclown.com/core-module-flashing.html)

Firmware for node is here [https://github.com/bigclownlabs/bcf-generic-node](https://github.com/bigclownlabs/bcf-generic-node)

### MQTT

Commands can be sent only to nodes powered by the power module, or usb-gateway.

#### LED

  * On
    ```
    mosquitto_pub -t "node/{id}/led/-/state/set" -m true
    ```
  * Off
    ```
    mosquitto_pub -t "node/{id}/led/-/state/set" -m false
    ```
  * Get state
    ```
    mosquitto_pub -t "node/{id}/led/-/state/get" -n
    ```

#### Relay on Power module
  * On
    ```
    mosquitto_pub -t 'node/{id}/relay/-/state/set' -m true
    ```
    > **Hint** First aid:
    If the relay not clicked, so make sure you join 5V DC adapter to Power Module

  * Off
    ```
    mosquitto_pub -t 'node/{id}/relay/-/state/set' -m false
    ```
  * Get state
    ```
    mosquitto_pub -t 'node/{id}/relay/-/state/get' -n
    ```

#### Relay module
  * On
    ```
    mosquitto_pub -t "node/{id}/relay/0:0/state/set" -m true
    mosquitto_pub -t "node/{id}/relay/0:1/state/set" -m true
    ```
  * Off
    ```
    mosquitto_pub -t "node/{id}/relay/0:0/state/set" -m false
    mosquitto_pub -t "node/{id}/relay/0:1/state/set" -m false
    ```
  * Get state
    ```
    mosquitto_pub -t "node/{id}/relay/0:0/state/get" -n
    mosquitto_pub -t "node/{id}/relay/0:1/state/get" -n
    ```

#### Led Strip on Power module
  Beware, it works only on remote nodes.

  * Brightness, the value is in percent of the integer:
    ```
    mosquitto_pub -t 'node/{id}/led-strip/-/brightness/set' -m 50
    ```
  * Color, standart format #rrggbb and non standart format for white component #rrggbb(ww)
    ```
    mosquitto_pub -t 'node/{id}/led-strip/-/color/set' -m '"#250000"'
    mosquitto_pub -t 'node/{id}/led-strip/-/color/set' -m '"#250000(80)"'
    ```
  * Compound, format is [number of pixels, fill color, ... ], example rainbow effect
    ```
    mosquitto_pub -t 'node/{id}/led-strip/-/compound/set' -m '[20, "#ff0000", 20, "#ff7f00", 20, "#ffff00", 20, "#00ff00", 20, "#0000ff", 20, "#960082", 24, "#D500ff"]'
    ```
  * Effects
    * Test
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"test"}'
      ```
    * Rainbow
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"rainbow", "wait":50}'
      ```
    * Rainbow cycle
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"rainbow-cycle", "wait":50}'
      ```
    * Theater chase rainbow
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"theater-chase-rainbow", "wait":50}'
      ```
    * Color wipe
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"color-wipe", "wait":50, "color":"#800000"}'
      ```
    * Theater chase
      ```
      mosquitto_pub -t 'node/{id}/led-strip/-/effect/set' -m '{"type":"theater-chase", "wait":50, "color":"#008000"}'
      ```
  * Thermometer effect
    ```
    mosquitto_pub -t 'node/{id}/led-strip/-/thermometer/set' -m '{"temperature": 22.5, "min":-20, "max": 50}'
    ```


#### LCD module
  * Write text, supported font size [11, 13, 15, 24, 28, 33], default font is 15, color can by true or false, default is true
    ```
    mosquitto_pub -t "node/{id}/lcd/-/text/set" -m '{"x": 5, "y": 10, "text": "BigClown"}'
    mosquitto_pub -t "node/{id}/lcd/-/text/set" -m '{"x": 5, "y": 40, "text": "BigClown", "font": 28}'
    mosquitto_pub -t "node/{id}/lcd/-/text/set" -m '{"x": 5, "y": 10, "text": "BigClown", "color": true}'
    ```

  * Clear
    ```
    mosquitto_pub -t "node/{id}/lcd/-/screen/clear" -n
    ```

#### Radio
  Read more here [bc-gateway](https://github.com/bigclownlabs/bch-usb-gateway)


## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT/) - see the [LICENSE](LICENSE) file for details.

---

Made with &#x2764;&nbsp; by [**HARDWARIO s.r.o.**](https://www.hardwario.com/) in the heart of Europe.
