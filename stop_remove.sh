#!/bin/bash

docker stop mts
docker rm mts
docker network rm serverNet
