C application to detect and track objects on Raspberry pi.
It uses userland, tensorflow and opencv libraries as dependencies

jetson dependencies instalation guide:
cmocka
sudo apt install libcmocka-dev
libv4l2
sudo apt install libv4l-dev
libsdl2
sudo apt install libsdl2-dev
sudo apt install nvidia-l4t-jetson-multimedia-api
-lnvbuf_fdmap not found:
sudo ln -s /usr/lib/aarch64-linux-gnu/tegra/libnvbuf_fdmap.so.1.0.0 /usr/lib/aarch64-linux-gnu/tegra/libnvbuf_fdmap.so

to build:
```bash
make -j 4
```
