# Epad

epad is a virtual controller design to work together with Linux User input devices.

This project was based on two other similar projects:

* https://github.com/jehervy/node-virtual-gamepads
* https://github.com/sbidolach/mobile-gamepad

Both projects implement the same approach that epad does. But instead of nodejs, epad uses Elixir and Phoenix Framework.

## retropie

The main use case is to be able to run epad as virtual controller to play video game emulation using [retropie](https://retropie.org.uk/).

### Installing and configuring epad on retropie

To install epad in retropie, there are basic Linux commands:

* First we need to install Erlang on raspberrypi (retropie):
```
sudo apt-get update
sudo apt-get install libssl-dev
sudo apt-get install ncurses-dev
curl -o otp_src_23.0.tar.gz http://www.erlang.org/download/otp_src_23.0.tar.gz
tar -xzvf otp_src_23.0.tar.gz
cd otp_src_23.0/
./configure
make
sudo make install
cd ..
rm otp_src_23.0.tar.gz
sudo rm -R otp_src_23.0/
```
* The second step is to download the epad release for raspberrypie4:
```
curl -o /tmp/epad-0.1.0-raspberrypi4.tar.gz
sudo mkdir /opt/epad
sudo tar zxf /opt/epad-0.1.0-raspberrypi4.tar.gz -C /opt/epad
```
* After that, copy the epad config for retropie:
```
sudo cp /opt/epad/priv/retropie/MobileGamePad.cfg /opt/retropie/configs/all/retroarch-joypads/
```
* Install the systemd service file and run epad using systemd
```
sudo cp /opt/epad/priv/systemd/epad.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable epad.service
sudo systemctl start epad.service
```
* Get your phone and access `http://retropie-IP:4000`. You should be able to select the `controller` and interact with the retropie user interface.

## Debugging uinput events

* Installing input-utils tool
```
sudo apt-get install input-utils
```
* Dump out all the input devices
```
sudo lsinput
```
* Display events
```
sudo input-events [number]
```

## Development

This project follows the standard mix commands. The attention point is when testing and creating releases. If you are targeting a specific platform (like raspberrypi4) then make sure you source and configure your toolchain before calling `mix release`.

To release and cross compile:
```
mix phx.digest
MIX_PROD=prod mix release
```

To start your local development environment:

  * Install dependencies with `mix deps.get`
  * Install Node.js dependencies with `npm install` inside the `assets` directory
  * Start Phoenix endpoint with `iex -S mix phx.server`

## TODO

* Finish keyboard and touchpad implementations
* Add metrics layer
* Phoenix Framework cleanup

## Contributing

Feel free to submit any PR. I will be happy to review and accept.

## License

[MIT](https://spdx.org/licenses/MIT.html)

Parts of javascript frontend files and _priv/retropi/MobileGamePad.cfg_ has borrowed from `mobile-gamepad`.
