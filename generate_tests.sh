#!/bin/bash
# This script generates test cases for the project.

./sauvola image.jpg image.png -b -w 15 -k 0.03 -r 128 | tee single_image_banchmark.txt 
./sauvola imageDirectory outputDirectory -b -w 45 -k 0.05 -r 1024 | tee directory_banchmark.txt
