#!/bin/bash

sudo ip addr flush dev enp0s31f6
sudo ip addr add 10.0.0.$1/24 dev enp0s31f6
sudo ip link set enp0s31f6 up

echo "Configuracao concluida"

