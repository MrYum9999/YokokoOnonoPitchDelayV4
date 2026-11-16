# Yokoko Onono Pitch Delay v4

A model of an analogue pitch-shifting delay: four MN3005 bucket-brigade chips (16,384 stages) on a microcontroller clock, with a compander, a resonant filter in the feedback loop and a three-step pitch sequencer.

## Getting the plugin (GitHub builds it)

1. Upload this folder's contents to a GitHub repository, including the hidden `.github` folder.
2. Open the **Actions** tab. "Build Windows VST3" runs by itself and takes about 10 minutes.
3. Open the finished run (green check) and download **YokokoOnono-v4-VST3** under Artifacts.
4. Extract the zip and copy the **Yokoko Onono Pitch Delay v4.vst3** folder into `C:\Program Files\Common Files\VST3\`.
5. Rescan plugins in your DAW.

v4 is a separate plugin, so it installs next to v1 and v3 and your existing projects are untouched.

## New in v4

- **reverse** (along the bottom of the front): the sound is cut into pieces one repeat long, and each piece is played backwards while the next one is recorded. When the song is playing, the pieces start on the beat.
  - **off**: forwards, like v3.
  - **everything**: you hear only backwards. What you play and every repeat come out reversed.
  - **echoes**: what you play stays forwards. The repeats come back reversed.
  - **every other echo**: the repeats take turns: backwards, forwards, backwards...
- Everything else still works with reverse on: intervals, glide, random notes, count bars, modulation, slowdown.
- Reversing takes time: a piece can't be played backwards until it has been recorded, so the first reversed sound arrives about one repeat later than a forwards echo would.
- The window is taller to fit the new row. Reverse is saved in memory slots and preset files.

## New in v3

- **6ths and 9ths** on interval 1 and 2 (15 positions, same note still at noon).
- **Preset files**: "save to file" writes a `.yokoko` file wherever you like (your song folder, say), named whatever you call the file. "open file" loads one. The name shows next to "preset" and is saved with your DAW project.
- **random notes**: replaces interval 1 and 2. Every time the sequence reaches step 2 or 3 it picks a new interval. It listens to the note you're playing ("hearing A") and only picks intervals that land on a note in the key.
- **make it weird**: random notes picks odd intervals instead (2nds, tritones, 7ths, 11ths, 13ths), and sometimes lands just outside the key on a blue note or a borrowed note.
- **key**: the key of your song, any of the 12 notes, major or minor.
- **count bars** (on the back): for odd time signatures. The bar is split into beat groups (7/8 as 2+2+3, 11/8 as 3+3+3+2 and so on). Each group is one step, and the sequence starts again on every downbeat (steps go 1, 2, 3, 1 ... when there are more than three groups). The step-length switches on the front become **time signature** and **beat groups**. With "follow the song" on, the time signature comes from your DAW, including changes mid-song. Otherwise drag the top number and click the bottom one. Click the beat groups to try other groupings. One repeat is one beat of the time signature: an eighth in 7/8, a quarter in 5/4.
- **Idle whine fixed**: the modelled clock bleed made a very quiet high tone that changed pitch with every step, even in silence. It is now gone, except in Slowdown, where it only sounds while there is audio.

## The front

| Control | What it does |
|---|---|
| mix | Dry and wet. Noon is both at full. |
| filter | The resonant low-pass inside the feedback loop. Each repeat passes through it again. |
| feedback | How much comes back. Past about three quarters it starts to run away on its own. |
| glide | How long the clock takes to slide to a new interval. All the way down is an instant jump. |
| interval 1, interval 2 | The pitch of steps 2 and 3: 2 octaves down to 2 octaves up, including 3rds, 4ths, 5ths, 6ths, octaves and 9ths. Same note at noon. |
| random notes, make it weird, key | See "New in v3". |
| preset | save to file / open file. |
| step 1/2/3 | How long each step lasts: quarter, dotted eighth or eighth. |
| memory | one, now, two. Pick one or two and press **save here** to keep the current settings there. Switching to it later brings them back. |
| time | Drag the number, or tap the **tap** circle. With "follow the song" on, it uses your DAW's tempo. |
| on | Effect on or off. |
| reverse | off, everything, echoes, every other echo. See "New in v4". |

How the pitch works: like the real hardware, one clock drives the chips. When a step changes the clock, whatever is already inside comes out faster or slower, so you hear it at the new pitch. With feedback up, each repeat gets shifted again on the next step.

## The back (click "turn it over")

- **follow the song**: use the DAW tempo. Tapping turns it off.
- **modulation**: interval 1 and 2 become LFO speed and depth. The step switches become shape (triangle, sine, square), randomness (none, some, lots) and what moves (pitch, filter, both).
- **step**: the sequence stops. Tap, or send a MIDI note, to move to the next step.
- **slowdown**: halves the clock. Up to 32 seconds of delay, with aliasing and clock whine while sound is passing through.
- **ramp the filter / feedback / glide**: that knob sweeps up and down by itself. Mix becomes ramp speed.
- **trails**: repeats keep ringing when you switch the effect off.
- **momentary**: the on circle only works while held.

Every control can be automated in your DAW, which covers what the pedal's CV and expression inputs would do.

## Building it yourself instead

Install Visual Studio 2022 Community (Desktop development with C++), CMake 3.22+ and Git, then in this folder:

```
cmake -B build -A x64
cmake --build build --config Release
```

Built with [JUCE](https://juce.com/) 9.0.5. Font: Courier Prime (SIL Open Font License, see `Resources/OFL.txt`).
