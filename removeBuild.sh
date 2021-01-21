#!/bin/bash

docker stop $(docker ps -a -q)
docker rm $(docker ps -qa)
docker build -t mts .
