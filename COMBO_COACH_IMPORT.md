# Practice a Lucky Stats combo in CE

This guide covers CE builds with Combo Coach's folder auto-import support.
The implementation is in the [CE fork PR](https://github.com/0xburn/TrainingMode-CommunityEdition/pull/1);
it is not yet part of an official Community Edition release.

You can automatically open your latest practice export, or choose a file through
Training Lab's usual Import menu. Both methods use the same `.gci` file.
Practice export on Lucky Stats is currently available to site admins only.

## Create the practice file

1. Open the game in Lucky Stats and choose **Clip**.
2. Enable **Practice (.gci)** and choose **Play as**.
3. Mark **Start** and **End** around the sequence. You can also select MP4/GIF.
4. Download the practice file. For automatic loading, save it directly in the
   folder you configured below.

**Practice this moment** is also available for exporting from a paused moment.
The converter may start earlier to restore a supported state; the export result
reports that lead-in.

## Automatic: save to your practice folder, then start CE

### One-time setup on Windows or macOS

1. Create a folder you can easily find, such as `Downloads/CE Practice/USA`.
   The final folder should be named `USA` for this NTSC build.
2. Stop the running game in Dolphin. Open **Config/Settings → GameCube**.
3. Set **Slot A** to **GCI Folder**. Use its folder configuration control to
   select your `CE Practice/USA` folder.
4. Start CE once and finish any initial memory-card setup it requests. Configure
   your controller if needed.

That chosen folder is the virtual memory card. No second copying/import step or
desktop helper is needed. Changing the folder does not move saves from your old
card; they remain at their previous location. Dolphin's folder settings apply
to that Dolphin profile.

### Each time

1. Save the downloaded `.gci` directly in the configured `USA` folder.
2. Wait for the download to finish.
3. Start CE in Dolphin. If CE is already running, stop the game and start it again.
4. CE enters Training Lab, loads the newest compatible Lucky Stats practice
   export, and starts its demonstration. After the demonstration, D-pad Down
   chooses a nearby hit and D-pad Left retries the countdown.

No special command-line flag is required for the folder-enabled build. Downloading
a file does not launch Dolphin. The browser may ask where to save; choose the
configured folder rather than leaving the file in the top-level Downloads folder.

“Newest” means the time Lucky Stats created the export, stored inside the file.
Renaming or moving it does not make it newer. Files exported at the same
millisecond use their unique IDs as a stable tie-breaker. Older exports made
before folder support remain available through manual Import; export again to
make one eligible for automatic loading.

Automatic import runs once per game startup. Returning to character select later
does not automatically import again. Unrelated saves are ignored. If there is no
eligible readable file, CE keeps normal menu navigation. A file whose import
header is incompatible is skipped; another eligible export can be selected.
This is not a guarantee that arbitrarily corrupted recording bodies can load.

**To skip automatic loading, hold B while starting CE until its normal menu
appears, then release B.** You can now use other events or select any file manually.

## Traditional: choose a file in Training Lab

### With a GCI-folder memory card

1. With the game stopped, save/copy the `.gci` into Dolphin's configured GCI folder.
2. Start CE. If using a new automatically recognized export, hold **B** during
   startup to skip automatic loading, then release it at the normal menu.
3. Open **Training Lab**. At character select, choose **Import**.
4. Choose **Slot A**, select the recording, and confirm loading it.
5. Let the demonstration finish before selecting a hit for takeover.

The Import menu shows the recording's embedded name, which can differ from its
Finder/Explorer filename. Leaving your character unselected exposes recordings
for either character. You can use Slot B manually if that is where you keep imports;
automatic loading checks Slot A only.

### With a traditional `.raw` memory card

1. Stop the game. Open Dolphin's **Tools → Memory Card Manager**.
2. Open the `.raw` card configured for your chosen slot and import the `.gci`.
3. Start CE and follow **Training Lab → Import → your slot → recording → confirm**.
   Hold **B** during startup if using an automatically recognized export on Slot A.

Simply placing a `.gci` beside a `.raw` file does not import it into that card.

## Troubleshooting

- **It loads an older practice file:** check that the new download finished and
  is in the exact folder shown in Dolphin. Stop/start the game to refresh the
  virtual card. Old exports need to be re-exported for automatic recognition.
- **It stays in the menu:** check that you have the folder-enabled CE build, that
  Slot A is configured correctly, and that B is released. Try manual Import to
  see whether CE recognizes the recording.
- **You have many files:** archive practice files outside the configured folder
  when no longer needed. A GCI folder still has a virtual memory card's capacity
  and file-count limits; it is not an unlimited library.
- **The folder picker rejects the folder:** select the `USA` subfolder. In Dolphin
  versions that do not expose the GCI folder picker, use the traditional flow or
  update Dolphin. An existing GCI-folder override can supersede the normal path.

## Maintainer notes

The browser stamps only the GCI directory entry: `TMREC_L7_<13-digit Unix ms>_<8 hex ID>`
as the internal name and seconds since 2000 in the card timestamp. The encoded
recording, checksums, and original game data remain unchanged. Stock manual
import still recognizes the `TMREC` prefix.

The event hook scans Slot A, selects a supported header, and invokes CE's existing
event handler. A one-shot request is consumed by the Training Lab CSS hook before
using the normal import path. The explicit developer-launcher marker
`TMREC_L7AUTOLOAD` takes priority over folder candidates, retaining compatibility
with `launch-coach.py --autoload-latest`.

The fork's normal build includes this behavior. Maintainers can compile without
startup hooks using `L7_AUTOLOAD=0 ./build.sh /path/to/melee.iso release`.

Dolphin references:

- [Folder picker and region validation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/Settings/GameCubePane.cpp).
- [GCI folder mapping](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/Config/MainSettings.cpp).
- [Card loading and capacity handling](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GCMemcard/GCMemcardDirectory.cpp).
