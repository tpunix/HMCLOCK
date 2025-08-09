
FONT_H = sfont.h sfont16.h font50.h font66.h

all: bdfont eptest

bdfont:
	gcc -Wall -g -I/mingw64/include/freetype2 -o bdfont bdfont.c -lfreetype -lz

eptest: epd_gui.c $(FONT_H)
	gcc -Wall -g -o eptest epd_gui.c gdifb.c -lgdi32

$(FONT_H): bdfont mkfont.sh
	./mkfont.sh

clean:
	rm -f bdfont.exe eptest.exe $(FONT_H)

