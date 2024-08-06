#!/bin/bash

uhubctl -l 1-1 -p 3 -a off

sleep 5

uhubctl -l 1-1 -p 3 -a on
