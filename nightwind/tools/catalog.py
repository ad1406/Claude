"""The motifs of the Night Wind sonata, as search specifications for match.find, grouped in families.
Intervals in semitones. modes: exact / tonal (±1 semitone, same direction) / octave (mod 12) / contour (step-skip-leap class).
levels: surface, 0.5, 1, 2 (beats per reduction window), bar. collapse: merge repeated pitches. gap: embellishing notes
allowed between motif notes. beatmin/beatmax: spacing of the motif's notes in beats."""

def long_first(C, ns, h):
    """the first note is held (the sigh's long note): at least 1.5x the next onset distance"""
    d0 = ns[0]["q_end"] - ns[0]["q0"]
    return d0 >= 1.5 * (ns[2]["q0"] - ns[1]["q0"]) - 1e-6

def strong_start(C, ns, h):
    b = ns[0]["bar"] - 1; pos = (ns[0]["q0"] - C.mq[b]) / C.beat[b]
    return abs(pos - round(pos)) < 1e-3

FAMILIES = [
  dict(id="call", name="The Call", color="#d62f2f", germ="bar 1",
       desc="Bar 1, ff all'improvisa: a falling third whose lower note is struck twice (G–E–E), in triplets, thrown down "
            "through four octaves. The wind's 'loud' voice."),
  dict(id="song", name="The Song", color="#1f6fd1", germ="bars 2–4",
       desc="Bars 2–4, tranquillo: a held note sinking to its lower neighbour and rising (E–D♯–E–F♯), a climb in "
            "'two steps up, one back' (G–A–F♯–G–A–B), and a swinging fourth (B–E–B–E). The wind's 'plaintive' voice."),
  dict(id="descent", name="The Descent", color="#1a9a5b", germ="bar 2 (bass)",
       desc="The line that answers the Song from below in bar 2: E–D–C♯–C…, stepwise and largely chromatic, falling "
            "while the Song rises."),
  dict(id="wind", name="Wind figures", color="#8a4fc4", germ="bars 38, 80, 96, 108",
       desc="The accompanying 'weather': the Allegro's quaver ostinato, the arching arpeggio of the second theme, "
            "the fluttering pedal of the Giocondamente. Each is built from the three germs."),
]

MOTIFS = [
  # ---- The Call
  dict(id="call", family="call", name="Call", label="falling third + repeated note",
       spec=dict(iv=[-3, 0], modes=["tonal"], transforms=["P", "I"], rhythm=[1, 1], rtol=0.3, levels=["surface"], sides=["top", "bottom"],
                 sources=["v", "h"], beatmax=1.6),
       desc="G–E–E. Found transposed, diatonic (major or minor third), inverted (rising third + repeat), augmented (the Allegro's "
            "dotted-crotchet call, b. 38) and diminished; also in the accompaniment, where each beat of bars 2–8 is a call "
            "(chord, then the lower note twice)."),
  dict(id="callwide", family="call", name="Widened call", label="falling fourth + repeated note",
       spec=dict(iv=[-5, 0, 0], modes=["exact"], transforms=["P"], rhythm=[1, 1, 1], rtol=0.3, levels=["surface"], sides=["top"],
                 sources=["v", "h"], beatmin=0.45, beatmax=1.6),
       desc="The call's third widened to a fourth, the lower note struck again and again (B–E–E–E, b. 526)."),
  dict(id="hammer", family="call", name="Hammered call", label="repeated notes, then the falling third",
       spec=dict(iv=[0, 0, -1, -3], modes=["tonal"], transforms=["P"], levels=["surface"], sides=["top"], sources=["v", "h"], beatmax=0.55),
       desc="The repeated note moved in front of the third and multiplied (A♯–A♯–A♯–A♯–A–F♯ at the quasi cadenza, b. 561; "
            "D–D–D–D–C♯–A♯, b. 618): the call as a summons."),
  # ---- The Song
  dict(id="sigh", family="song", name="Sigh", label="held note, lower neighbour, rise",
       spec=dict(iv=[-1, 1, 2], modes=["exact", "tonal"], transforms=["P", "I"], levels=["surface", 0.5], sides=["top", "bottom"],
                 sources=["v", "h"], gap=0, beatmax=4.1, filter=long_first),
       desc="E (held)–D♯–E–F♯. Exact and tonal (whole-tone neighbour) forms; inverted (upper neighbour, falling)."),
  dict(id="climb", family="song", name="Climb", label="two steps up, one back",
       spec=dict(iv=[2, -3, 1, 2, 2], modes=["tonal", "contour"], transforms=["P", "I"], levels=["surface"], sides=["top", "bottom"],
                 sources=["v", "h"], gap=0, beatmax=1.1, chain=True, chain_overlap=2),
       desc="G–A–F♯–G–A–B. In Part II it is diminished to semiquavers and chained, so that whole runs (b. 285, 534, 717) are "
            "built from it; inverted it descends the same way (b. 286)."),
  dict(id="swing", family="song", name="Fourth swing", label="oscillating fourth",
       spec=dict(iv=[5, -5, 5], modes=["exact"], transforms=["P", "I"], levels=["surface", 1], sides=["top", "bottom"],
                 sources=["v"], rhythm=[1, 1, 1], rtol=0.25, beatmin=0.45),
       desc="B–E–B–E (b. 4), F♯–B–F♯–B (b. 8): the Song's cadence. Also its inversion (E–B–E–B) and octave-displaced forms."),
  dict(id="ascent", family="song", name="Stepwise ascent", label="five or more notes rising by step, in long values",
       spec=dict(iv=[1, 1, 1, 1], modes=["contour"], transforms=["P"], levels=[1, 2, "bar"], sides=["top", "bottom"],
                 sources=["v", "h"], collapse=[True], beatmin=0.9, beatmax=6, chain=True, fold=True),
       desc="The Song's rising line in its structural form, one step per beat or slower (E–F♯–G–A–B over bars 2–4), wherever "
            "it is carried, in melody or bass."),
  dict(id="songfull", family="song", name="Song (whole phrase)", label="the complete theme",
       spec=dict(iv=[-1, 1, 2, 1, 2, -3, 1, 2, 2], modes=["tonal"], transforms=["P"], levels=["surface"], sides=["top", "bottom"],
                 sources=["v", "h"], gap=1, maxcost=2.5, maxspan=12, beatmax=3.1),
       desc="Sigh and climb as one phrase: the theme's literal returns, in E, F, F♯, B, C and E again."),
  # ---- The Descent
  dict(id="chrom", family="descent", name="Chromatic descent", label="five or more notes falling by semitones",
       spec=dict(iv=[-1, -1, -1, -1], modes=["exact", "octave"], transforms=["P", "I"], levels=["surface", 0.5, 1, 2], sides=["top", "bottom"],
                 sources=["v", "h", "all"], collapse=[True], beatmin=0.24, beatmax=4.1, chain=True),
       desc="The counterline's chromatic form, and its inversion (the chromatic bass that climbs under the Allegro's "
            "transition, A♯–B–C–C♯–D, b. 72, and E–F–F♯ under the fff of b. 76). Searched in the structural lines too (beat by beat, the highest or lowest "
            "note), so it is found when each tone is ornamented: in b. 496 the bass A–G♯–G–F♯–F–E sounds each tone with "
            "its lower neighbour an octave higher."),
  dict(id="counter", family="descent", name="Counterline cell", label="E–D–C♯–C",
       spec=[dict(iv=[-2, -1, -1], modes=["exact"], transforms=["P"], levels=[0.5, 1, 2], sides=["bottom", "top"],
                  sources=["v", "h"], collapse=[True], fold=True, beatmin=0.45, beatmax=2.1)],
       desc="The bass of bar 2 exactly: a whole step and two semitones down (E–D–C♯–C), on the beats. It returns in the "
            "structural layer of later passages, e.g. the bass of b. 528 (E–D–C♯–C again, each on a beat)."),
  dict(id="steps", family="descent", name="Stepwise descent", label="six or more notes falling by step, in long values",
       spec=dict(iv=[-1, -1, -1, -1, -1], modes=["contour"], transforms=["P"], levels=[1, 2, "bar"], sides=["top", "bottom"],
                 sources=["v", "h"], collapse=[True], beatmin=0.9, beatmax=6, chain=True, fold=True),
       desc="The counterline's diatonic form, at beat level or slower: the chorale descents of Part II (b. 293), the "
            "basses under the Song."),
  dict(id="circle", family="song", name="Circling", label="double neighbour around one tone",
       spec=dict(iv=[1, -1, -2, 2], modes=["tonal"], transforms=["P", "I"], levels=["surface"], sides=["top", "bottom"],
                 sources=["v", "h"], beatmax=0.55, chain=True, chain_overlap=3),
       desc="B–C–B–A–B (susurrando, b. 336): the Sigh's neighbour note doubled into a figure that circles its tone; it "
            "is the murmuring layer of the Fantasia (b. 336, 571) and of the Giocondamente's pedal."),
  # ---- Wind figures
  dict(id="ostinato", family="wind", name="Allegro ostinato", label="fifth, fourth, then a scale arch",
       spec=[dict(iv=[7, 5, 2, 1, 2, -2, -1], modes=["tonal"], transforms=["P", "I"], levels=["surface"], sides=["bottom", "top"], sources=["v", "h"], beatmax=1.1),
             dict(iv=[12, 7, 2, 1, 2, -2, -1], modes=["tonal"], transforms=["P"], levels=["surface"], sides=["bottom"], sources=["v", "h"], beatmax=1.1)],
       desc="E–B–E–F♯–G–A–G–F♯…: the Allegro's quaver bass, a swinging fourth (the Song's cadence) opened into a climb and a descent."),
  dict(id="arch", family="wind", name="Arch", label="arpeggio rising three steps, falling back",
       spec=dict(iv=[3, 4, 5, -5, -4], modes=["tonal", "contour"], transforms=["P", "I"], levels=["surface", 1], sides=["top", "bottom"],
                 sources=["v", "h"], rhythm=[1, 1, 1, 1, 1], rtol=0.3, beatmin=0.9, beatmax=3.1),
       desc="The second theme's head (b. 80, F♯–A–C♯–F♯–C♯–A) and, augmented, the bass of the Stentato (b. 108)."),
  dict(id="pedal", family="wind", name="Fluttering pedal", label="melody tone over a neighbour oscillation",
       spec=dict(iv=[-5, -2, 2, -2, 2], modes=["exact", "tonal"], transforms=["P", "I"], levels=["surface"], sides=["top", "bottom"],
                 sources=["v", "h"], beatmax=0.4),
       desc="A–E–D–E–D–E (Giocondamente, b. 96): a melody note, then the Sigh's neighbour figure in perpetual motion."),
]
