#!/bin/bash

docker network create --subnet=10.10.0.0/16 serverNet
docker build -t mts .
