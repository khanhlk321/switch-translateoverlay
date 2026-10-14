Screen Translate is a Tesla overlay. Open it on the frame you want. It reads English or Japanese on that frame and shows Vietnamese in the panel.

Build from the devkitPro shell, in this folder:

    pacman -S switch-dev switch-curl
    make

Copy the results onto the SD card:

    translate.ovl -> sd:/switch/.overlays/translate.ovl
    dist/switch/translate/en-vi.tsv -> sd:/switch/translate/en-vi.tsv
    dist/switch/translate/ja-vi.tsv -> sd:/switch/translate/ja-vi.tsv

The Tesla menu has to be installed already. Its combo (L + D-pad down + right stick click, unless you changed it) opens this overlay and translates the current frame into Vietnamese. X translates again. Y switches the source language between English and Japanese. B closes the panel and returns to the game.

If the console is online, the overlay sends the recognized text to the MyMemory translation service and shows the Vietnamese sentence. If the console is offline, or the request fails, it uses the word list for that source language on the SD card.
