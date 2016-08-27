# MIDI2KEY

MIDI2KEY is a very small utility to accept note ON and note OFF events from all MIDI input devices installed on a the user's computer and translating them into key strokes applicable to LOTRO (http://www.lotro.com) music system commands.

## Backstory

So, a few days ago (from Apr 28 2007) I started playing LOTRO and immediately fell in love with the music system. I found the keyboard input a little awkward to use, though, as it seems many of you have. I wanted to use my MIDI-capable keyboard instead. So today I wrote a small utility that accepts MIDI data in real-time from all MIDI input devices on your computer and translates the notes into something applicable to LOTRO.

## MIDI files to ABC files

If you're looking to convert MIDI files into ABC files playable on LoTRO's music system, this is not the tool you need. Try this one created by Digero!

## Usage

These instructions assume you already know how to connect your MIDI keyboard to your computer and have already done so.

1. Download.
2. Unzip to anywhere.
3. Run program and follow the instructions that appear on the window for mapping your keys in-game (unless you want your own keymapping, in which case edit the accompanying .ini file).
4. Then, with the utility running in the background, start up the music system in LOTRO and play your MIDI keyboard.

## Release

Originally released at [https://www.lotro.com/forums/showthread.php?50704-MIDI2KEY-MIDI-keyboard-to-LOTRO-music-system-utility-(v0-9-0-0)](https://www.lotro.com/forums/showthread.php?50704-MIDI2KEY-MIDI-keyboard-to-LOTRO-music-system-utility-(v0-9-0-0)).

## Updates/Revision History

### Release notes for v0.9.0.4:

- Added ability to customize key mapping via a MIDI2KEY.ini file in the same directory as the executable. Several example ini files are included. To use one of them, rename it to "MIDI2KEY.ini" and restart the program. The program will use a default key mapping if no "MIDI2KEY.ini" file is present.
- This version allows a full 3-octave mapping by using one of the supplied .ini files as mentioned above.

### Release notes for v0.9.0.3:

- Includes a simple arpeggiator function. When enabled, simply hold down several keys on your MIDI keyboard for the arpeggiator to walk through them. You may choose to limit the arpeggiator to only the lower-octave notes by checking the "Freehand high octave" option. Also, by default, the arpeggiator walks down the note scale but you can have it walk up the scale instead by checking the "Crawl Up" option.
- Included some basic filtering for notes so that the same keyboard event is not sent to the application multiple times simultaneously. (This was only creating lag issues and didn't actually affect the music being played since it was essentially the same note overlapped.)

### Release notes for v0.9.0.2:

- Included the use of Q, the note "B" just below middle-C.
- Included option to alternate or not alternate each octave.

### Release notes for v0.9.0.1:

- Remapped octaves such that each octave alternates the use of LOTRO's low or high-octave note sets. This creates a more "varied" sound more easily.

### Release notes for v0.9.0.0:

- Initial release.
