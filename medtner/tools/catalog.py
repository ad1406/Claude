"""The motifs that are searched for. Intervals in semitones; with exact=False, interval classes are compared
(a step is a step, a third a third), so diatonic and chromatic transpositions of the same idea count. rhythm =
relative inter-onset intervals; scale = allowed size relative to the beat (augmentation/diminution)."""

CATALOG = {
    "nw": [
        dict(id="call", name="Wind call", color="#d1495b", kind="melodic-rhythmic",
             desc="The opening outcry (bar 1): a falling minor third with the lower note struck twice, in triplet quavers, "
                  "usually cascading down through the octaves. The piece's motto: it starts the Introduction, the Allegro, "
                  "the development's Largamente interruptions, and the turn back to the Introduction tempo.",
             **{"def": dict(iv=[-3, 0], exact=True, rhythm=[1, 1], rtol=0.3, scale=(0.15, 0.8), lines=("upper",), inv=False, tailgap=1.8)}),
        dict(id="intro", name="Introduction theme", color="#b5651d", kind="melodic",
             desc="Bars 2–4 as one phrase: the neighbour sigh run straight into the turn cell (E–D♯–E–F♯–G–A–F♯–G–A–B). "
                  "The sonata's cyclic theme: it comes back a semitone higher in F minor (250), a tone higher in F♯ (362, cantando), "
                  "and finally in the bass of the closing tenebroso run (717).",
             **{"def": dict(iv=[-1, 1, 2, 1, 2, -3, 1, 2, 2], exact=False)}),
        dict(id="sigh", name="Neighbour sigh", color="#edae49", kind="melodic",
             desc="Bar 2, tranquillo: a held note, its lower chromatic neighbour, the note again and a step up (E–D♯–E–F♯). "
                  "The same semitone pivot governs the tonal plan (E → E♭ minor episode → F minor return of the Introduction → E).",
             **{"def": dict(iv=[-1, 1, 2], rhythm=[2.5, 0.5, 0.5], rtol=0.6, exact=False)}),
        dict(id="turn", name="Turn cell", color="#00798c", kind="melodic",
             desc="Bar 3: up a step, down a third, then three steps up (G–A–F♯–G–A–B). The Introduction's continuation; "
                  "diminished into semiquavers it becomes the engine of the Allegro molto sfrenatamente (bar 285) and of the closing "
                  "tenebroso run (bar 717).",
             **{"def": dict(iv=[2, -3, 1, 2, 2], exact=False, maxioi=1.1)}),
        dict(id="sfren", name="Sfrenato run", color="#30638e", kind="melodic",
             desc="Bar 285: the turn cell embedded in a rising semiquaver scale (D♯–E–F♯–G–A | F♯–G–A–B). "
                  "Head of the second large part; returns at Tempo I (534) and in the coda (717).",
             **{"def": dict(iv=[1, 2, 1, 2, -3, 1, 2, 2], exact=False, maxioi=0.6)}),
        dict(id="wind", name="Wind ostinato", color="#6a4c93", kind="accompaniment",
             desc="Bar 38, left hand: the 15/8 quaver wave (E–B–E–F♯–G–A–G–F♯…) that carries the Allegro's first group, "
                  "an arch of rising fifth/fourth then scale, i.e. the turn cell's scale turned into perpetual motion.",
             **{"def": dict(iv=[7, 5, 2, 1, 2, -2, -1], exact=False)}),
        dict(id="stheme", name="Second theme", color="#2a9d8f", kind="melodic",
             desc="Bar 80, Tranquillo (D major): broken-triad melody in dotted crotchets (A–C♯–F♯–C♯–A–F♯…); "
                  "recapitulated in G major at bar 204.",
             **{"def": dict(iv=[5, -5, -4, -3, 3, 4], exact=False)}),
        dict(id="gioc", name="Giocondamente pedal", color="#e76f51", kind="figuration",
             desc="Bar 96: melody tones over a fluttering semiquaver pedal (A–E–D–E–D–E), the closing-group texture.",
             **{"def": dict(iv=[-5, -2, 2, -2, 2, 5], exact=False, maxioi=0.4, gap=4, inv=False)}),
        dict(id="descent", name="Chorale descent", color="#8d6a9f", kind="melodic",
             desc="Long-note stepwise descent through six or more scale degrees (e.g. bar 293, G–F♯–E–D–C–B…). "
                  "The augmented, inverted shadow of the rising scales: the 'falling' half of the wind.",
             **{"def": dict(iv=[-1, -2, -2, -2, -1, -2], exact=False, minioi=0.9, maxioi=2.1, inv=True)}),
        dict(id="teneb", name="Tenebroso knocking", color="#264653", kind="rhythmic",
             desc="Bar 372 (and the Coda at 646): repeated-note pairs sliding down by semitones (B B, B B, B B, A♯ A♯, A A…). "
                  "The Wind call's repeated note, slowed into a chromatic lament over a rising arpeggiated bass.",
             **{"def": dict(iv=[0, -1, 0, -1, 0, -1, 0], exact=True, maxioi=1.2, inv=False)}),
        dict(id="lament", name="Chromatic descent", color="#7a7a7a", kind="melodic",
             desc="Six or more notes falling by semitones in moderate values (not fast runs): the lament line, in melody or bass.",
             **{"def": dict(iv=[-1, -1, -1, -1, -1], exact=True, minioi=0.3, inv=False, gap=3)}),
    ],
    "mi": [
        dict(id="threat", name="Threat motif", color="#d1495b", kind="melodic-rhythmic",
             desc="Bar 1: double-dotted crotchet, semiquaver, dotted quaver, semiquaver, falling C–A♭–G–F (scale degrees 5–♭3–2–1). "
                  "The fugue subject (bar 210) is this motif made chromatic (F♯–D–C♯–C).",
             **{"def": dict(iv=[-4, -1, -2], rhythm=[1.75, 0.25, 0.75], rtol=0.45, scale=(0.3, 3))}),
        dict(id="ddrhythm", name="Double-dotted rhythm", color="#edae49", kind="rhythmic",
             desc="The threat's rhythm alone (7 : 1 : 3, double-dotted crotchet, semiquaver, dotted quaver), whatever the pitches: "
                  "it is also the left hand's rising counter-line in bar 1 (A♭–B♭–C–D), so it saturates both hands.",
             **{"def": dict(iv=None, n=4, rhythm=[1.75, 0.25, 0.75], rtol=0.3, scale=(0.4, 2.5))}),
        dict(id="cantsigh", name="Cantabile sigh", color="#2a9d8f", kind="melodic-rhythmic",
             desc="Bar 59, dolce cantando: long A♭, a quick C above, falling to G. The same long–short anacrusis as the threat, "
                  "turned into a lyrical appoggiatura; the second group's melody. Recapitulated at 326.",
             **{"def": dict(iv=[4, -5], rhythm=[11, 1], rtol=0.35, exact=False, scale=(0.1, 0.4))}),
        dict(id="arp", name="Second-group arpeggio", color="#b5651d", kind="accompaniment",
             desc="Bar 59: the semiquaver arch under the cantabile melody (F–A♭–C–D–E♭–F and back), a ninth chord unfolded; "
                  "recapitulated a fifth higher at 326.",
             **{"def": dict(iv=[3, 4, 2, 1, 2, -1, -2, -2, -4], exact=False, maxioi=0.4)}),
        dict(id="toll", name="Tolling octaves", color="#30638e", kind="rhythmic",
             desc="Bar 54 (9/8): one pitch alternating in octaves in semiquavers, a bell or drum stroke under the 9/8 theme; "
                  "returns at 113–116, 140–142, 156–160 and in the recapitulation (321).",
             **{"def": dict(iv=[12, -12, 12, -12], exact=True, maxioi=0.6, inv=False)}),
        dict(id="knock", name="Repeated-note knock", color="#264653", kind="rhythmic",
             desc="Three or more repeated notes in quick values: the misterioso C♯ ostinato before the fugue (182–186) and its "
                  "relatives. The same 'knocking' idea that drives the Night Wind's Tenebroso.",
             **{"def": dict(iv=[0, 0, 0], exact=True, maxioi=0.6, gap=6, inv=False)}),
        dict(id="octa", name="Octatonic wedge", color="#6a4c93", kind="harmonic",
             desc="Bars 87–90 (sempre al rigore di tempo): a five-note cell (+3 +4 −3 +5 −8 semitones) sequenced up by minor thirds, "
                  "so the passage stays inside one octatonic collection: symmetry as menace.",
             **{"def": dict(iv=[3, 4, -3, 5, -8], exact=True)}),
        dict(id="hexa", name="Minaccioso cell", color="#8d6a9f", kind="harmonic",
             desc="Bar 91, minaccioso: interlocking minor-third/semitone cells (+3 +1 +4 −3 +4) climbing by major thirds, "
                  "a hexatonic sweep that opens the C-minor section.",
             **{"def": dict(iv=[3, 1, 4, -3, 4], exact=True)}),
        dict(id="coda", name="Coda figure", color="#e76f51", kind="figuration",
             desc="Bar 393ff: a semiquaver cell (−9 +2 −2 −3 +7) sequenced downward by semitones, the Coda's sempre animato motor.",
             **{"def": dict(iv=[-9, 2, -2, -3, 7], exact=False)}),
        dict(id="lament", name="Chromatic descent", color="#7a7a7a", kind="melodic",
             desc="Six or more notes falling by semitones in moderate values (not fast runs): the lament line, in melody or bass.",
             **{"def": dict(iv=[-1, -1, -1, -1, -1], exact=True, minioi=0.3, inv=False, gap=3)}),
    ],
}
