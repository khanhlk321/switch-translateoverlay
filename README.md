Screen Translate is a Tesla overlay. Open it on the frame you want. It reads the English on that frame and shows Vietnamese or Japanese in the panel.

Build from the devkitPro shell, in this folder:

    pacman -S switch-dev switch-curl
    make

Copy the results onto the SD card:

    translate.ovl -> sd:/switch/.overlays/translate.ovl
    dist/switch/translate/en-vi.tsv -> sd:/switch/translate/en-vi.tsv
    dist/switch/translate/en-ja.tsv -> sd:/switch/translate/en-ja.tsv

The Tesla menu has to be installed already. Its combo (L + D-pad down + right stick click, unless you changed it) opens this overlay and translates the current frame. X translates again. Y switches between Vietnamese and Japanese. B closes the panel and returns to the game.

If the console is online, the overlay sends the recognized English to the MyMemory translation service and shows that result. If the console is offline, or the request fails, it uses the word list on the SD card. The service needs Wi-Fi. The word lists do not.

Screenshot capture on firmware before 10.0.0 needs debug mode. The reader matches the console's standard font, so plain interface text works better than stylized logos.
