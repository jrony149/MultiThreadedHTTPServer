#!/bin/bash

sudo docker run -p $1:8085 --net=serverNet --ip=10.10.0.2 --name="mts" mts ./httpserver4 8085 -l "$2" -N "$3"
