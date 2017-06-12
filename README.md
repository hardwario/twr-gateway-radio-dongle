<a href="https://www.bigclown.com"><img src="https://s3.eu-central-1.amazonaws.com/bigclown/gh-readme-logo.png" alt="BigClown Logo" align="right"></a>

# USB gateway Firmware

[![Travis](https://img.shields.io/travis/bigclownlabs/bcp-usb-gateway/master.svg)](https://travis-ci.org/bigclownlabs/bcp-usb-gateway)
[![Release](https://img.shields.io/github/release/bigclownlabs/bcp-usb-gateway.svg)](https://github.com/bigclownlabs/bcp-usb-gateway/releases)
[![License](https://img.shields.io/github/license/bigclownlabs/bcp-usb-gateway.svg)](https://github.com/bigclownlabs/bcp-usb-gateway/blob/master/LICENSE)
[![Twitter](https://img.shields.io/twitter/follow/BigClownLabs.svg?style=social&label=Follow)](https://twitter.com/BigClownLabs)

This repository contains firmware for USB gateway.

## Firmware Programming
```
dfu-util -s 0x08000000:leave -d 0483:df11 -a 0 -D firmware.bin
```
More information about dfu [here](https://doc.bigclown.com/core-module-flashing.html)

Firmware for node is here [https://github.com/bigclownlabs/bcp-generic-node](https://github.com/bigclownlabs/bcp-generic-node)

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

#### LCD module
  Beware, it works only usb-gateway

  * Write text, supported font size [11, 13, 15, 24, 28, 33], default font is 15
    ```
    mosquitto_pub -t "node/{id}/lcd/-/text/set" -m '{"x": 5, "y": 10, "text": "BigClown"}'
    mosquitto_pub -t "node/{id}/lcd/-/text/set" -m '{"x": 5, "y": 40, "text": "BigClown", "font": 28}'
    ```

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT/) - see the [LICENSE](LICENSE) file for details.

---

Made with ❤ by [BigClown Labs s.r.o.](https://www.bigclown.com) in Czech Republic.
