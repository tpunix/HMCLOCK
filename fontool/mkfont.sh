#!/bin/sh
# 请自行下载文泉驿正黑字体，或用其他字体替换。

./bdfont.exe -l small.txt wqy-zenhei.ttc -s 14 -h sfont.h sfont
./bdfont.exe -l small.txt wqy-zenhei.ttc -s 16 -h sfont16.h sfont16

./bdfont.exe -l dseg.txt DSEG7CR.ttf -s 50 -h font50.h F_DSEG7_50
./bdfont.exe -l dseg.txt DSEG7CR.ttf -s 66 -h font66.h F_DSEG7_66

