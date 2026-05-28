#!/bin/bash
# This script generates test cases for the project.

./sauvola image.jpg image.png -b -w 35 -k 0.05 | tee single_image_banchmark.txt 
./sauvola imageDirectory outputDirectory -b -w 25 -k 0.08 | tee directory_banchmark.txt
