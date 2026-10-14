Screen Translate is a Tesla overlay. Open it on the frame you want. It reads the English on that frame and shows Vietnamese in the panel. After the dictionary is on the SD card, translation does not use the network.

Build from the devkitPro shell, in this folder:

    pacman -S switch-dev
    make

Copy the results onto the SD card:

    translate.ovl -> sd:/switch/.overlays/translate.ovl
    dist/switch/translate/en-vi.tsv -> sd:/switch/translate/en-vi.tsv

The Tesla menu has to be installed already. Its combo (L + D-pad down + right stick click, unless you changed it) opens this overlay and translates the current frame. X translates again. B closes the panel and returns to the game.

The dictionary file is a word list under CC BY-SA 3.0 and the GNU Free Documentation License, from Wiktionary (Matthias Buchmeier, 2015-01-02). Common game phrases in the overlay replace those entries when both exist.

Screenshot capture on firmware before 10.0.0 needs debug mode. The reader matches the console's standard font, so plain interface text works better than stylized logos.
