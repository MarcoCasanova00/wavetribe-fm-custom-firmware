# Frankentribe OS — FM-1 Groovebox Firmware

TRY IT LIVE: https://marcocasanova00.github.io/wavetribe-fm-custom-firmware/

<p align="center">
  <img src="docs/logo.png" width="192" height="208" alt="Frankentribe OS logo">
</p>

**Experimental. Unrefined by design.**

A single-file sequencer / 4-operator FM synth / drum kit hybrid. Sounds pulled from EPROM dumps, game SFX, and third-party banks — no studio polish, deliberate grit.

## What it is
- **Synth:** Real 4-op FM engine (`fm_engine.c`) with 8 algorithms, 8 operator waveforms, per-op envelopes, LFO + ENV pages.
- **Drums:** classic drum-machine kits (kick-heavy default) + a game-SFX kit — 8-bit DOS SFX resampled to gritty 16-bit (not polished loops).
- **Sequencer:** Pattern + live MIDI, Shift+click button latching for combos. sampler-inspired deeper **swing** (up to ~62%), step-resolution readout, and **SAVE / SOUNDS** slots (8 flash slots, persisted to browser localStorage in the web emulator).
- **Generative genre engine:** seeded rules fill the three synth rows (bass/chord/lead) per genre — TECHNO / HOUSE / DUBSTEP / D&B / ELECTRO / AMBIENT / LOFI / ACID — via the **COMPOSE** page (`GLO+ARP`; build with OCT+, one-step undo with OCT-, per-bar **drift** `GLO+PLAY` with an OFF/ON/LOCK cycle). It applies genre defaults (BPM, swing, accent, FX, arp flavour) and is deterministic (same seed → same pattern). **Key & scale overrides:** on COMPOSE, **SELECT** picks the key (tonic) and **PRESETS** the scale (major/minors/dorian/phrygian/lydian/mixolydian/pentatonics/blues/chromatic); both re-generate live and join the deterministic seed, and a chosen scale **derives a fresh chord progression from it** rather than the genre's static one. Externally drivable over MIDI CCs 94–97 and 101. The **ARP** button opens a dedicated per-part arpeggiator page (on/off, mode, octaves, rate); **EUCLID** (`GLO+SEQ`) writes Euclidean gate rhythms to any part.
- **Osc modes:** per-synth-part **hard sync**, **ring/AM**, **phase distortion** and **vector synthesis** (OCT+ on the WAVE/EDIT page, or CC 98) for buzzy/metallic/morphing 80s textures.
- **LOST bank:** a sixth curated bank of hand-authored "lost ROMpler" patches (FM-EP, Round Bass, Glass Pad, …).
- **Stutter FX:** loop-roll stutter as a fourth FX mode (OFF/CHORUS/ECHO/COMBO/STUT).
- **Frankentribe lab skin:** colorful multi-accent UI — yellow→green header band, per-page color banners and accent rails, color-coded part rows, status-pill footer.
- **Live FX layer:** hold FX + keys (F3..C4, D4/E4 throws) for momentary whole-mix effects — stutter, reverse/echo throws, tape stop, bitcrush, gate, filter sweep, half speed. Also triggerable by MIDI CC 99 (0–7 start / 127 stop).
- **Quick mute:** hold HOME (≥300 ms) + F3…C4 toggles part mutes live (tap HOME still opens the system page).
- **Bidirectional MIDI clock:** MASTER mode sends 24 PPQ clock + Start/Stop over USB; **SLAVE** mode follows external Start/Continue/Stop and steps the sequencer on incoming F8 (USB or TRS). Clock mode cycles via GLO+FX or CC 100.
- **MORPH / VMORPH osc modes:** phase distortion and vector synthesis whose amounts follow the note envelope — crisp attack settling to a sine (MORPH) or a per-note four-wave sweep (VMORPH) — alongside SYNC/RING/PD/VEC as per-part integrated oscillator modes.
- **Soundbook:** GLO+HOME opens a browse of every sound by name + category — patches by bank, drum samples by kit, wave libraries by origin.
- **Font:** normalized to Terminus 16 (regular body + generated bold for titles/brand/selected part); Nerd Fonts not compatible with the 1-bit BDF bitmap pipeline.
- **FUNC keys:** with GLO latched, the first 16 white keys act as grid-key shortcuts (clear / save / load / pattern ± / copy / paste / step mode).
- **Patch/sound selection:** category-first browsing (GLO+PRESET = sound, GLO+OCT- = category). **SAVE** opens a SOUNDS page: `SLOT / SOURCE / ACTION (LOAD|SAVE|CLEAR)` with OCT+ to confirm.
- **Transport on the FX page:** KNOB1 = tempo (60–250 BPM), KNOB2 = swing, KNOB3 = accent, KNOB4 = loop length — no need to enter GLO.
- **UI:** HOME screen with theme palette ("default" = olive/magenta; plus AMBER, CYAN, RED, ALIEN, LAVA).

## Provenance — where the material came from, honestly

### FM voices (byte-level decode)
- 128 vintage FM voices decoded byte-for-byte from a 1980s multi-timbral sound-module EPROM.
- 144 voices decoded from another classic FM sound-module ROM (per the authoritative editor format: 8-byte operators, inverted Total Level, OP4..OP1 storage). Algorithm routing approximated by carrier-set mapping.
- 64 extra FM voices from a well-known early-80s synth (names + heuristic params, not byte-exact).
- 32 curated single-cycle waves mined from rare synth/drum-machine EPROM dumps (mostly untapped).

### Drum / wave sources
- Drum one-shots reconstructed from a mobile drum-app's classic machine samples.
- Further drum kits mapped from classic sample-triggered hardware.
- Single-cycle wavetables imported from a softsynth bank collection.
- One kit built from 8-bit/11025 Hz SFX pulled from a 1995 public-domain DOS game (kick/snare/clap/chh/ohh), deliberately lo-fi.

### Ideas only (no code/data taken)
- "Four knobs per page" groovebox interaction design from an open-source sibling project.
- Chorus/echo FX-chain concepts from guitar multi-effects units.
- A mobile groovebox app was reviewed but not integrated.

### Visual skin
- Palette, mascot sprite and font sheets from the same 1995 public-domain DOS game (decoded with REA + an open-source engine; no code taken — colour/theme assets only).

See [`INFLUENCES.md`](INFLUENCES.md) for per-source fidelity notes (bytes / params / assets / ideas).

## What's experimental / unrefined
- The 4-op module algorithm topology is mapped, not bit-exact (voices sound correct but not hardware-identical).
- The game-SFX kit is 8-bit, resampled, clipped/gain-varied — deliberately lo-fi.
- Bank selection is number-based (OCT- / OCT+) — functional, not polished.
- Genre generator state (genre/seed/density) is **not persisted** in slots — it resets on boot.
- Synth chords are a block triad across three consecutive steps (parts are monophonic).
- Docs: `docs/GUIDE.md` (web-emulator guide), `docs/HANDOFF.md` (state), `docs/PATCHES.md` (MIDI CC map).

## Build
Host (test): `cc -DDINGHY_HOST ...`
Web: `sh tools/build_web.sh` → `/docs/` → GitHub Pages.

## Device build / flashing

`python3 tools/build.py` (Linux x86-64, the JieLi toolchain and AC79 SDK — see
`tools/get_toolchain.sh`) produces `build/dinghy.fwsc` (FM-1 identity; `--board smk37`
with `--model pro|elite|mke` for the SMK-37 family). Install it with the M-Vave
updater or a Web-MIDI SysEx installer.

The bundled update loader is conservative: it writes **only the app area**
(never the SPL/head, calibration/BTIF, stock USR resources or key_mac), checks the
package's key before touching flash, skips already-equal sectors, verifies each
sector and the full-package CRC, and is resumable across power loss (the update
record survives; a damaged package leaves the loader waiting). `bootguard.h`
adds a recovery/ROM fallback after repeated failed boots. Rollback to stock
remains possible (its resources are kept and protected).

Live (hard-refresh `Ctrl+Shift+R`): [https://MarcoCasanova00.github.io/frankentribe-os/](https://marcocasanova00.github.io/wavetribe-fm-custom-firmware/)

---
*Brand: FRA N K E N T R I B E OS — built from scraps, redeems nothing.*

## Catalog of sounds

Every sound below is selectable in the firmware: browse them in the
**Soundbook** (`GLO+HOME`), or pick patches on the SOUNDS page / CC 93.

### Patch banks (397 patches)

**APP** — 13 patches
- Init
- Bright
- Dark
- Pluck
- Soft
- Bell
- SawLead
- Organ
- Strings
- NoiseBass
- Chimes
- PWM
- Acid

**Vintage FM** — 64 patches
- SYN-LEAD 2
- SYN-LEAD 3
- SYN-LEAD 4
- SYN-LEAD 5
- SYN-CLAV 1
- SYN-CLAV 2
- SYN-CLAV 3
- SYN-PIANO
- SYNBRASS 1
- SYNBRASS 2
- SYNORGAN 1
- SYNORGAN 2
- SYN-VOX
- SYN-ORCH
- SYN-BASS 1
- SYN-BASS 2
- HARP-FLUTE
- BELL-FLUTE
- E.P-BRS BC
- T.BL-EXPA
- CHIME-STRG
- B.DRM-SNAR
- SHIMMER
- EVOLUTION
- WATER GDN
- WASP STING
- LASER GUN
- DESCENT
- OCTAVE WAR
- GRAND PRIX
- ST.HELENS
- EXPLOSION
- PICCOLO
- FLUTE   2
- OBOE
- CLARINET
- SAX BC
- BASSOON
- STRINGS 4
- STRINGS 5
- STRINGS 6
- STRINGS 7
- STRINGS 8
- BRASS   4
- BRASS   5
- BRASS 6 BC
- BRASS   7
- BRASS   8
- RECORDER
- HARMONICA1
- HRMNCA2 BC
- VOICE   2
- VOICE   3
- GLOKENSPL
- VIBE    2
- XYLOPHONE
- CHIMES
- GONG    1
- GONG    2
- BELLS
- COW BELL
- BLOCK
- FLEXATONE
- LOG DRUM

**ROM** — 32 patches
- Voice_Rom2-0
- Voice_Rom2-1
- Voice_Rom2-2
- Voice_Rom2-3
- ms404 v1.0
- ms404 v1.0
- ms404 v1.0
- ms404 v1.0
- Workstation-0
- Workstation-1
- Workstation-2
- Workstation-3
- FX Unit v1.06
- FX Unit v1.06
- FX Unit v1.06
- FX Unit v1.06
- DrumMachine_roms_2-0
- DrumMachine_roms_2-1
- DrumMachine_roms_2-2
- DrumMachine_roms_2-3
- MemorySynth_v2
- MemorySynth_v2
- MemorySynth_v2
- MemorySynth_v2
- LeadSynth_1_(27c40
- LeadSynth_1_(27c40
- LeadSynth_1_(27c40
- LeadSynth_1_(27c40
- MiniMono_64_TomMora
- MiniMono_64_TomMora
- MiniMono_64_TomMora
- MiniMono_64_TomMora

**4OP-A** — 128 patches
- GrandPiano
- Uprt piano
- Deep Grd
- HonkeyTonk
- Elec Grand
- Fuzz Piano
- SkoolPiano
- Thump Pno
- LoTine81Z
- HiTine81Z
- ElectroPno
- NewElectro
- DynomiteEP
- DynoWurlie
- Wood Piano
- Reed Piano
- PercOrgan
- 16 8 4 2 F
- PumpOrgan
- <6 Tease>
- Farcheeza
- Small Pipe
- Big Church
- AnalogOrgn
- Thin Clav
- EZ Clav
- Fuzz Clavi
- LiteHarpsi
- RichHarpsi
- Celeste
- BriteCelst
- Squeezebox
- Trumpet81Z
- Full Brass
- Flugelhorn
- ChorusBras
- FrenchHorn
- AtackBrass
- SpitBoneBC
- Horns BC
- MelloTenor
- RaspAlto
- Flute
- Pan Floot
- Bassoon
- Oboe
- Clarinet
- Harmonica
- DoubleBass
- BowCello
- BoxCello
- SoloViolin
- HiString 1
- LowString
- Pizzicato
- Harp
- ReverbStrg
- SynString
- Voices
- HarmoPad
- FanfarTpts
- HiString 2
- PercFlute
- BreathOrgn
- NylonGuit
- Guitar #1
- TwelveStrg
- Funky Pick
- AllThatJaz
- HeavyMetal
- Old Banjo
- Zither
- ElecBass 1
- SqncrBass
- SynFunkBas
- ElecBass 2
- AnalogBass
- Jaco Bass
- LatelyBass
- MonophBass
- StadiumSol
- TrumptSolo
- BCSexyPhon
- Lyrisyn
- WarmSquare
- Sync Lead
- MellowSqar
- Jazz Flute
- HeavyLead
- Java Jive
- Xylophone
- GreatVibes
- Sitar
- Bell Pad
- PlasticHit
- DigiAnnie
- BaadBreath
- VocalNuts
- KrstlChoir
- Metalimba
- WaterGlass
- BowedBell
- >>WOW<<
- Fuzzy Koto
- Spc Midiot
- Gurgle
- Hole in 1
- Birds
- MalibuNite
- Helicopter
- Flight Sim
- Brthbells
- Storm Wind
- Alarm Call
- Racing Car
- Whistling
- Space Talk
- Space Vibe
- Timpani
- FM Hi-Hats
- Bass Drum
- Tube Bells
- Noise Shot
- Snare 1
- Snare 2
- Hand Drum
- Synballs
- Efem Toms

**4OP-B** — 144 patches
- Brass
- Horn
- Trumpet
- LoStrig
- Strings
- Piano
- NewEP
- EGrand
- Jazz Gt
- EBass
- WodBass
- EOrgan1
- EOrgan2
- POrgan1
- POrgan2
- Flute
- Piccolo
- Oboe
- Clarine
- Glocken
- Vibes
- Xylophn
- Koto
- Zither
- Clav
- Harpsic
- Bells
- Harp
- SmadSyn
- Harmoni
- SteelDr
- Timpani
- LoStrg2
- Horn Lo
- Whistle
- ZingPlp
- Metal
- Heavy
- FunkSyn
- Voices
- Marimba
- EBass 2
- SnareDr
- RD Cymb
- Tom Tom
- Mars to
- Storm
- Windbel
- rom2
-  p    K
-  PP ^  F
-  P  V  4
-  p  W
- 1P`AZ  5
- #P     D
- Pp    D
- ;p B
- Horn2
- Horn3
- Horns
- Flugelh
- Trombon
- Trumpt2
- Brass2
- Brass3
- HradBr1
- HardBr2
- HardBr3
- HardBr4
- HuffBrs
- PercBr1
- PercBr2
- String1
- String2
- String3
- String4
- SoloVio
- RichSt1
- RichSt2
- RichSt3
- RichSt4
- Cello1
- Cello2
- LoStrg3
- LoStrg4
- LoStrg5
- Orchest
- 5th Str
- Pizzic1
- Pizzic2
- Flute2
- Flute3
- Flute4
- Pan Flt
- SlowFlt
- 5th Flt
- Oboe2
- Bassoon
- Reed
- Harmon2
- Harmon3
- Harmon4
- MonoSax
- Sax 1
- Sax 2
- rom4
-  P  T
-  p q
-  ` s   S
-  P A
-  ` r   x
-  P q
-  p0E
- (p u   f
- (` v   f
-  p z[
- 
-  0Pp   $
-  p @   u
-  p C  JU
-  ` B   U
- JOrgan1
- JOrgan2
- COrgan1
- COrgan2
- EOrgan3
- EOrgan4
- EOrgan5

**LOST** — 16 patches
- FM-EP
- Soul Bass
- Finger Bass
- Glass Pad
- Warm Strings
- Breath Brass
- Tine Clav
- Space Bell
- Chiff Organ
- Airy Choir
- Pulse Lead
- Saw Stack
- Deep Pluck
- Shimmer
- Sub Drop
- Magic Marimba

### Drum kits (6 kits, 5 samples each)

**APP**
- KICK
- SNARE
- CLAP
- CHH
- OHH

**Kick Machine**
- KICK
- SNARE
- CLAP
- CHH
- OHH

**Dance Machine**
- KICK
- SNARE
- CLAP
- CHH
- OHH

**Rhythm Box**
- KICK
- SNARE
- CLAP
- CHH
- OHH

**Sample Machine**
- KICK
- SNARE
- CLAP
- CHH
- OHH

**Preset Machine**
- KICK
- SNARE
- CLAP
- CHH
- OHH

### Wave libraries

- **WAVELIB** — 256 factory single-cycle waves
- **EPROM mine A** — 64 mined single-cycle waves
- **EPROM mine B** — 64 mined single-cycle waves

