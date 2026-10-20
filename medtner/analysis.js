// Analysis text. {38} or {38-44} links to bars of the current piece; {mi:210} / {nw:250} link across pieces;
// {m:call} links to a motif (shows all its instances).
window.MEDTNER_TEXT = {

nw: {
  lede: `Composed 1910–11 and dedicated to Rachmaninoff; published with the <i>Sonata-Skazka</i> as Op. 25. Its motto is
  Tyutchev's poem <i>“What are you wailing about, night wind?”</i>, a poem about the wind singing of the primordial chaos
  beneath the night's calm. (This edition doesn't print the epigraph.) It is one continuous movement of 723 bars, about 32 minutes in this recording, in two large parts joined
  by an <i>attacca</i>. Part I is an Introduction and a sonata-form Allegro that ends by returning to the Introduction.
  Part II is a Fantasia that reworks the same material, then a Coda built from the Introduction.`,

  sections: [
    [1, 37, 1, "Introduzione", "Andante ♩ = 50 · 5/4–4/4", "E minor",
     `The first bar holds the whole piece in miniature. A <i>ff</i>, <i>all'improvisa</i> {m:call}: G–E–E, falling
     through three octaves over a bare E-minor triad {1}. Then, <i>tranquillo</i>, the {m:intro}: a held E, its lower neighbour D♯, back to E,
     up to F♯ ({m:sigh}), running straight into the {m:turn} G–A–F♯–G–A–B {2-4}. Almost everything that follows is made from these
     three ideas: the falling third with a repeated note, the semitone neighbour, and the turn cell's scale.`],
    [1, 12, 2, "Motto and theme", "", "E minor → chromatic",
     `The Introduction theme's first statement is diatonic. From {6} it is restated a fourth lower (B), and the inner voices start sliding
     chromatically. By {9-12} the bass has fallen to A, C♯ and D♯, and the pitch content covers ten or eleven pitch classes per bar.
     B♭ and D♭ arrive in {12}: the tritone and its neighbour, which come back as structural keys later.`],
    [13, 21, 2, "First digression: the F-minor shadow", "ten. · cantabile dolce", "F → G",
     `{13} stands on an F bass (F–A♭–C with D and B): the Neapolitan of E, the semitone above the tonic. It is the first
     appearance of the harmonic idea that organises the whole sonata (E against F), and it is exactly the key in which the Introduction
     comes back at {250}. The sextuplet ascent over it is the turn cell's scale made fluid. <i>Cantabile dolce</i> {14} and the
     <i>legatissimo, espressivo</i> paragraph {15-18} lead to an <i>agitato</i> surge {19-21}.`],
    [22, 33, 2, "Climax of the Introduction", "ff · appassionato", "C pedal → E",
     `A <i>ff</i> C pedal {22} carrying E, B♭ and F♯ (the French-sixth sound, here a chord of tritones) begins the
     Introduction's own climax. {25} <i>f appassionato</i>, {31} <i>ff</i>. The texture is now full, but the melody keeps to
     steps and the semitone sigh.`],
    [34, 37, 2, "Poco a poco più mosso", "5/4", "E minor",
     `The motion speeds up into sextuplet arpeggio waves {36-37}, and the left hand already outlines the coming
     ostinato. The Introduction doesn't cadence; it accelerates into the Allegro.`],

    [38, 249, 1, "Allegro: sonata form", "♩. = 92–100 · 15/8", "E minor",
     `A full sonata form in 15/8: five dotted-crotchet beats, so the Introduction's 5/4 is kept at a faster pulse. The exposition runs
     {38-128}, the development {129-177} and the recapitulation {178-249}. The tonal plan is unusual: the second group is in D major
     (♭VII, not III) and recapitulates in G major, and the closing group reaches the tonic <i>major</i> (E major, {234}) before
     everything is pulled up a semitone into F minor.`],
    [38, 79, 2, "First group", "fz · pesante · cantando", "E minor",
     `The Allegro opens with the {m:call} again, at its original pitch {38-39}, over the {m:wind}, which is the turn cell's
     scale-arch turned into perpetual quavers. The <i>pesante</i> theme {40-44} climbs G–A–B (repeated B's)–C–D to E and falls
     back by step: the turn cell stretched into a whole phrase. Note the long–short <i>fz</i> chords that start each
     group of bars. A <i>cantando</i> continuation {42-47} and a weightier restatement {48-58} lead to a <i>ffz</i> outburst {59}
     and a wide-ranging <i>leggiero</i> paragraph {62-71}. The transition {72-79} is <i>molto pesante</i>, with triplet
     quavers in G minor / B♭ (the tritone region). It culminates in an <i>fff</i> chord of E, B♭ and F {76}: the tonic, its tritone and its upper
     neighbour struck together. It dissolves <i>sempre più tranquillo</i>, and the wind call (A–F♯–F♯) passes between the hands {79}.`],
    [80, 107, 2, "Second group", "Tranquillo ♩. = 72–80", "D major",
     `The {m:stheme} {80-83} is a broken-triad melody in dotted crotchets, pastoral and bell-like, in D major. Its intervals
     (fourths and thirds) are the motto's falling third turned into a melody. <i>Poco a poco scherzando</i> {84} brings dotted
     figures. A <i>svegliando, veloce</i> burst {93-95} breaks into the <i>Giocondamente</i> {96}: melody tones over a fluttering
     {m:gioc}. This texture is the wind ostinato's descendant: perpetual motion again, now playful. {104} <i>sdegnoso</i> and
     {105} <i>risoluto, pesante ff</i> darken it with B♭ and A♭ chords.`],
    [108, 128, 2, "Closing group", "Stentanto ♩. = 72 → Giocondamente ♩. = 92", "F♯ minor / D major",
     `{108-117} <i>Stentanto</i> ("labouring"): wide arpeggio waves, one per beat, over a slow bass. This is the night-wind image at its most literal,
     and the figure returns at the same point of the recapitulation {222}. It builds (<i>sempre cresc.</i>, <i>appass.</i>) into the
     <i>ff risoluto</i> Giocondamente {118}, an exuberant tutti, with ten-note rushes {125}, {127} and a <i>molto calando</i> that sinks to
     <i>pp</i> {128}.`],
    [129, 177, 2, "Development", "", "F♯ minor → E♭ minor → E minor",
     `{129} begins <i>ppp dolce</i> with the giocondamente figure in F♯ minor. Then the Introduction breaks in: <i>Largamente</i>, ♩. = 60, <i>fz all'improvisa</i> {135},
     with the {m:call} on a B-major chord (the dominant), then on A {139}. Between these, {136} and {140} the arpeggio waves of
     the Stentanto return, and {137}, {141} a <i>molto tranquillo</i> chorale in chords. {144} <i>Più mosso</i> passes call fragments
     (on F, D, A) through the bass. The goal is E♭ minor (six flats, {153}): <i>cantabile, placido</i> {154-157}, a pedal
     figure in B♭–E♭ fifths. E♭ is the semitone <i>below</i> the tonic, as F was the semitone above in {13}: the
     {m:sigh}'s lower neighbour turned into a key. From {156} <i>poco a poco ritornando al tempo I</i>, scherzando and
     risoluto fragments drive back. {172} restores the E-minor signature, and <i>non legato</i>, <i>f risoluto</i> and septuplet runs
     {174-177} prepare the recapitulation.`],
    [178, 249, 2, "Recapitulation", "Tempo I · ff", "E minor → G major → E major",
     `{178-179} the {m:call} and {m:wind}, now <i>ff</i>, then the pesante theme <i>molto pesante</i> {181}. The first group is
     compressed and reaches the second group via {197-203} (<i>f risonante</i>, <i>sempre più tranquillo</i>). The {m:stheme} comes back at
     {204} in G major (the relative major, the "textbook" key the exposition avoided), then the giocondamente {210}, and the Stentanto
     arpeggio waves again {222-229}, now <i>sempre più agitato</i>, reaching <i>ff appassionato</i> {230}. The closing group turns to
     E major {234} (<i>giocondamente</i>, then <i>Molto giocondamente</i> {241} with heavy <i>ff pesante</i> chords {243} and
     <i>con strepito</i> {246}). The tonic major is a false victory: the {m:call} enters on A♭–F {248-249} and pulls everything into F minor.`],

    [250, 284, 1, "Return of the Introduction", "Tempo dell'introduzione · 4/4", "F minor → E minor",
     `The Introduction returns a semitone higher, in F minor {250}: the {m:intro} note for note (F–E–F–G | A♭–B♭–G–A♭–B♭–C), the
     Neapolitan shadow of {13} now realised as a key. It is a structural sigh, E rising to F and then sinking back.
     {262} <i>teneramente</i> returns to the one-sharp signature. {267} <i>f agitato</i> and {270} <i>ff</i> rebuild the climax, with a 3/2 bar {272} and
     <i>molto allargando</i>. The <i>ff risonante</i> trill and fermata {276} sit on a B pedal with E and F both sounding: the dominant, with the
     semitone conflict in the middle of it. A <i>lento, perdendosi</i> sinks to <i>pp</i>. The <i>irresoluto</i> a tempo {277}, <i>sempre accelerando</i> {279} and
     <i>veloce</i> {281} work up the run that ends Part I on a <i>lunga</i> fermata and <i>Attacca</i> {284}.`],

    [285, 723, 1, "Part II: Fantasia and Coda", "", "E minor",
     `Part II reworks Part I's material rather than presenting new material. Its head motif, the {m:sfren}, is the
     {m:turn} diminished into semiquavers (G–A–F♯–G–A–B appears literally inside it). Its episodes recall the Introduction theme in
     new keys: F♯ {362}, B minor {442}, C minor {579}. Its large-scale shape is a rondo-like chain with a reprise at Tempo I {534}, a
     <i>quasi cadenza</i> on the tritone key {561}, and a Coda {646} that sinks back into the Introduction's material.`],
    [285, 371, 2, "Allegro molto sfrenatamente", "♩ = 84 · 4/4", "E minor",
     `<i>Sfrenatamente</i> means "unbridled". The {m:sfren} runs over pulsing chords {285-292}. The bass also
     answers with D♯–E–F♯, the sigh's semitone. {293-300} the right hand sings a {m:descent} (F♯–G…G–F♯–E–D–C–B–A–G) over the
     run in the left hand: rising and falling scales at once, the "wind" as two currents. Triplet basses {306}, <i>ff</i>
     {303}, <i>ff tumultuoso</i> {324}. {327} turns <i>p leggiero</i>, <i>carezzando</i> {330} into the murmuring <i>pp susurrando</i>
     {336}, in which the repeated B's foreshadow the Tenebroso knocking. {362} <i>cantando</i>: the {m:intro} in F♯, a tone above the
     original, <i>f allargando</i> {364}, then D major <i>leggiero</i> {368}.`],
    [372, 441, 2, "Tenebroso", "𝅗𝅥 = 60–66", "B minor",
     `The emotional centre of the Fantasia. The {m:teneb}: repeated-note pairs B–B, B–B, B–B, A♯–A♯, A–A, G♯–G♯… sliding down by
     semitones over a rising arpeggiated bass. It combines the wind call's repeated note with the sigh's semitone, slowed into a lament. {380-383}
     the bass descends by whole tones (D, C, B♭, A♭). {391} <i>Meno mosso, una corda, senza pedale</i>, then the <i>sempre crescendo ed agitato</i>
     build {397-403}, with <i>fz</i> peaks. {404} <i>a tempo</i>, {408} <i>ff</i>, septuplet runs {409-411}, and {412} <i>mf espressivo, dolente</i>, a chorale
     of sighs. {418} <i>f risoluto</i>, {436} <i>f pleno</i>, then <i>poco a poco ritenuto</i> and <i>diminuendo</i>.`],
    [442, 479, 2, "Con meditazione", "Meno mosso", "B minor → C minor",
     `<i>Slentando, cantabile</i> {442-445}: the {m:turn} (D–E–C♯–D–E–F♯) becomes a meditative melody, then trills in the bass
     {446-449}. The key signature disappears {450}: C minor, with trills and sustained chords. <i>Poco a poco crescendo e svegliando</i> {454}
     ("waking up"), {458} <i>f</i>, {462} <i>a tempo (non subito)</i>, accumulating to <i>f con strepito</i> {477}.`],
    [480, 533, 2, "Vertiginoso", "ff · p marcato il basso", "C♯ minor → A minor → E minor",
     `{480} four sharps, <i>ff</i>, with a D♯ pedal and grace-note basses (<i>marcato il basso</i>, {484}). {496} <i>vertiginoso, p leggiero</i>:
     arpeggiated semiquavers over leaping basses, chromatic and dizzying. {508}–{512} <i>f</i> to <i>ff</i>, {520} <i>ffz</i>. Back in one sharp, <i>molto
     vertiginoso</i> {522} with triplet semiquavers, then <i>pp leggiero</i> {526}, building up to {532} <i>ff tumultuoso</i>.`],
    [534, 560, 2, "Reprise (Tempo I)", "molto marcato", "E minor",
     `The {m:sfren} returns at its original pitch {534}, followed by the chorale descent {542-548} as at {293}. The reprise is
     compressed: <i>ffz</i> {553}, the triplet bass {555}, <i>poco allarg.</i> {559}, and a turn into B♭.`],
    [561, 645, 2, "Quasi cadenza and Concentrando", "fff · Pedale tenuto", "B♭ major → D major → G",
     `{561} <i>fff, quasi cadenza (fermato)</i>, with the pedal held from {561} to {564}: an F⁷ chord, the dominant of B♭. B♭ is the tritone of E,
     the farthest point from the tonic, prepared since the B♭ chords of {12} and {72}. Wind-call figures (A–G–E, G–G–G) and falling
     thirds float above it, <i>calmando</i>, <i>carezzando</i>, <i>leggierissimo</i> {567}. {571} <i>Tempo I, pp susurrando</i> in B♭ over the F
     pedal. {578} <i>Concentrando, ma sempre con moto</i> and {579} <i>cantabile</i>: the Introduction theme in C minor. {597} two sharps, <i>fz agitato</i>,
     {604} <i>ff molto agitato</i>, then two <i>affannato</i> ("breathless") waves, each <i>meno mosso → accelerando → ff</i> {616-627}. {628} <i>Largamente</i>,
     {631} <i>plenissimo</i> in G, {633} <i>poco a poco più mosso</i>, {634} <i>sempre più dolce</i>.`],
    [646, 723, 2, "Coda", "Tenebroso 𝅗𝅥 = 60", "A minor → E minor",
     `The {m:teneb} returns a whole tone lower, on A over a C bass {646-650}: the Coda begins as the Tenebroso did. It accelerates ({651}
     <i>sempre più mosso</i>) into <i>f acciacato</i> chords {667-672} full of crushed grace notes, a <i>dolente</i> line over triplet basses {673}, and
     <i>fz</i> hammer-strokes {681-684}. {685} <i>ff, sempre più sfrenamente</i>, then <i>al fine</i>: the rest of the piece is one long
     diminuendo. {709} <i>fz p, sempre dimin. al fine</i>. At {717} <i>pp tenebroso, legatissimo</i>, the {m:intro} rises in the bass from E, as a
     sfrenato run, through four octaves {717-720}, <i>perdendosi</i>. The last sonority {723} is E minor with D♯, F♯ and A♯ added: the tonic
     triad saturated with its neighbours and its tritone, the conflicts of the whole piece sounding together, <i>pp</i>.`],
  ],

  essays: [
    ["Thematic transformation", `
    <p>Medtner's method here is <b>generative</b>: three cells from bars 1–3 produce almost all the themes.</p>
    <ul>
     <li><b>{m:call}</b> (falling minor third with a repeated lower note). It articulates form. It opens the Introduction {1}, the Allegro {38}
     and the recapitulation {179}, interrupts the development {135}, {139}, sends the music up a semitone into F minor {249}, and
     its repeated note becomes the {m:teneb} ({372}, {646}).</li>
     <li><b>{m:sigh}</b> (semitone lower neighbour). It is the most important harmonic idea, working on three levels: melodically (E–D♯–E), as a
     chord colour (the D♯ and F♯ in the final chord), and as key relations (F minor {13}, {250}; E♭ minor {153}).</li>
     <li><b>{m:turn}</b> (step up, third down, steps up). Stretched, it gives the pesante theme {40}; in perpetual motion, the {m:wind} {38};
     in semiquavers, the {m:sfren} {285}; slowed, the meditation {442}; in the bass, the last bars {717}.</li>
    </ul>
    <p>Note also <b>registral cascades</b>: the call always descends through octaves, while the sfrenato and closing runs ascend through octaves.
    Falling and rising are the two directions of the wind.</p>`],
    ["Harmony and tonal plan", `
    <p>The home key E minor is defined less by its dominant than by <b>its chromatic neighbours</b>.</p>
    <ul>
     <li><b>F (semitone above)</b>: the F bass of {13}, the F-minor return of the Introduction {250}, the E/F clash inside the dominant trill {276}.</li>
     <li><b>E♭ (semitone below)</b>: the E♭-minor <i>placido</i> episode at the centre of the development {153}.</li>
     <li><b>B♭ (tritone)</b>: the B♭ chords of {12}, the B♭/G-minor <i>molto pesante</i> transition {72}, the <i>fff</i> E–B♭–F chord {76}, and
     the whole quasi-cadenza on F⁷→B♭ {561-596}.</li>
    </ul>
    <p>The non-tonic keys of the sonata form are also unusual. The second group is in D major (♭VII), not G, and only the recapitulation
    supplies G {204}. The Introduction theme's later returns trace a rising line: E {2} → F {250} → F♯ {362}, then B minor {442} and
    C minor {579}. The final chord {723} contains D♯, F♯ and A♯ around the E-minor triad. These are the semitone neighbours and the tritone, so the
    tonal tensions of the piece are absorbed into the tonic itself.</p>`],
    ["Rhythm and metre", `
    <p>The Introduction is in <b>5/4</b> (bar 1) and 4/4. The Allegro is in <b>15/8</b>, five dotted-crotchet beats, so the quintuple
    grouping survives at a faster tempo. Part II is in 4/4 and moves in semiquavers, so the bar-to-bar pulse becomes more regular as the
    piece becomes more "unbridled".</p>
    <p>Rhythmic motifs: the triplet repeated note of the {m:call}; the long–short <i>fz</i> chord that starts each group of
    the Allegro; constant quavers or semiquavers (the {m:wind}, {m:gioc}, {m:sfren}) as the "wind" layer under every theme; and the
    repeated-note pairs of the {m:teneb}. Medtner's tempo indications form a hierarchy of related tempi (♩.=92–100, 72–80, 88–92,
    72, 60) rather than contrasts. The tempo curve in the player shows how the recording handles them.</p>`],
  ],
},

mi: {
  lede: `The second of the two sonatas Op. 53 (the first is the <i>Sonata romantica</i>), written around 1929–31 and published in 1933.
  Medtner called it his "most contemporary composition". It is one movement of 448 bars (about 18 minutes here) and a
  rigorous sonata form. The exposition has three groups. The development contains a <b>fugue</b>, whose subject is the opening
  "threat" motif made chromatic. The recapitulation dissolves into <i>quasi cadenza</i> episodes. A coda drives to a tritone-to-tonic
  cadence in F major.`,

  sections: [
    [0, 90, 1, "Exposition: first group and transition", "Allegro sostenuto ♩ = 84–92 · 3/4", "F minor",
     `<i>Concentrando</i>. The {m:threat} {1-3}: C–A♭–G–F (scale degrees 5–♭3–2–1) in the right hand, in a double-dotted
     rhythm (7 : 1 : 3 : 1), against a left hand rising A♭–B♭–C–D in the same rhythm (the {m:ddrhythm}). The first chord is
     F minor with E and D natural over an A♭ bass: the tonic is present but unstable from the start.`],
    [0, 24, 2, "Threat and its echoes", "pp · accelerando agitato · ff fermata", "F minor",
     `The threat is echoed <i>pp</i> {4}, then <i>poco a poco accelerando, agitato e cresc.</i> {6} to an <i>ff allargando</i>
     fermata on F with E and B (the leading note and the tritone) {8}. {9-11} <i>leggiero vivo, con pedale</i>: triplet cascades
     8va. {14} <i>ff</i> arpeggio waves in C minor, {15} <i>quasi eco</i>, {16} <i>poco a poco calmando</i>, {18} <i>a tempo</i>, {21}
     <i>tranquillo</i>. At {24} the threat returns on E (E–C–B♭–A♭), with a <i>m.d.</i> triplet figure underneath.`],
    [25, 53, 2, "Risoluto: the agitated continuation", "f risoluto, animato (più mosso)", "F minor → E♭",
     `{25} triplet semiquaver basses under sf chords: the left hand outlines chromatic descending lines against the right hand's
     anacrusic chords. {29} <i>sempre animato</i>, {33} <i>cantabile</i>, {35} <i>p dolce ma sempre a tempo</i>, {38} <i>ff</i> to <i>p subito</i>, an
     8va cantabile on E♭. {43} <i>poco a poco crescendo agitato</i>, {46} <i>f appassionato</i>, {50} <i>ff</i>, {52} a trill, and {53} an
     <i>ff</i> arpeggio cadenza over E♭: a tritone superimposition (A-major arpeggio on an E♭ bass) that resolves to A♭.`],
    [54, 78, 2, "Second group (9/8)", "L'istesso tempo · 9/8", "A♭ major → E♭",
     `{54} 9/8, <i>f</i>: a falling major third C–A♭ (the threat's first interval) over {m:toll}, octave E♭'s like a bell
     or drum. {59} <i>dolce cantando ma sempre a tempo</i>: the {m:cantsigh} (a long A♭, a quick C, falling to G) over the {m:arp}, an
     unfolded B♭⁹. The threat's long–short anacrusis has become lyrical. {61} crescendo, {63} <i>f</i>, {65} <i>più dolce</i>, {68}
     <i>ma a tempo</i>, {75} <i>f</i>, {77} <i>sf sempre a tempo</i>.`],
    [79, 90, 2, "Sordamente: the threat in F♯ minor", "pp sordamente · una corda · 3/4", "F♯ minor → octatonic",
     `{79} <i>marcato, pp sordamente</i> ("muffled"): the {m:threat} on C♯ (C♯–A–G♯–F♯) over a left-hand semiquaver tremolo on F♯/D,
     i.e. in F♯ minor, the semitone <i>above</i> the tonic. This is the piece's antagonist key, and the fugue will be in it. {81}
     again on A. {82-83} <i>f tre corde, con fermezza</i>, a C♯ tremolo bass. {87-90} <i>sempre al rigore di tempo</i>: the {m:octa}, a
     five-note cell sequenced up by minor thirds, which stays inside one octatonic collection (C, D, E♭, F, F♯, A♭, A, B).`],
    [91, 139, 1, "Exposition: third group (minaccioso)", "p minaccioso", "C minor",
     `The key signature changes to three flats. {91} <i>minaccioso, marcato</i>: the {m:hexa}, semiquaver sweeps of minor
     thirds and semitones climbing by major thirds (hexatonic), over octave basses. The third group alternates <i>f</i>/<i>p</i> blocks
     {93-106}, reaching <i>ff</i> {107}. {109} <i>addolcito, cantando</i>, {111} <i>dolce</i>. {113-116} <i>sf secco</i>: the {m:toll} again,
     over chromatically descending chords. {117} <i>f subito</i>, {120} <i>calmando</i>, {121} <i>concentrando</i>, {125} <i>animato</i>.
     {131-132} <i>sempre diminuendo, tenuto senza Ped.</i>, {133} <i>p marcato</i>: the threat in the bass (C–A♭–G–F♯), {137} <i>ff</i>.`],
    [140, 290, 1, "Development", "", "chromatic → F♯ minor (fugue) → dominant pedal",
     `The key signature disappears at {140}. The development has three phases: a recombination of the exposition's
     ideas {140-181}, a <i>misterioso</i> preparation {182-209}, and a <b>fugue</b> {210-290} whose subject is the threat motif.`],
    [140, 181, 2, "Recombination", "senza Ped. · 9/8 · ff", "",
     `{140-142} tolling, <i>diminuendo, senza Ped.</i>. {144} triplets, {146} <i>pp</i>, {148} <i>marcato, sostenuto</i>, {149} <i>poco a poco crescendo</i>.
     The 9/8 theme returns developmentally {156-160} (B♭⁷ over D), <i>diminuendo e più tranquillo</i> {161}, {164} <i>poco languido,
     legatissimo</i>. Then {168} <i>pp poco a poco crescendo</i> to the 3/4 <i>ff</i> {173}. At {174} the threat (B–G–F♯–E) cuts through, and
     {176} <i>poco a poco diminuendo</i>.`],
    [182, 209, 2, "Misterioso", "p misterioso · senza Ped.", "C♯",
     `{182} <i>p misterioso, senza Ped.</i>: a {m:knock} ostinato on C♯ in the tenor (C♯–C♯–C♯4…), with fragments above. {188} <i>poco crescendo</i>,
     {190} <i>espressivo</i>, {192} trills, {194} <i>pp quasi eco</i>, {197} <i>sotto/sopra</i> hand-crossings. The threat motif keeps
     surfacing, already in its fugue-subject form, F♯–D–C♯–C {198}, then on D {206-207}. {205}, {209} <i>sostenuto pp</i>.`],
    [210, 290, 2, "Fugue", "sempre al rigore di tempo · p marcato", "F♯ minor",
     `<b>Subject</b> {210-213}, left hand alone: (C♯–)F♯–D–C♯–C | F♯–E–C–C–B | E–D–B♭… The {m:threat}'s rhythm, with its intervals
     chromaticised (−4 −1 −1), followed by a semiquaver tail of chromatic wedges. <b>Countersubject</b> {214-219}, right hand, <i>marcato pp,
     sempre portamento e leggiero</i>: semiquavers in contrary motion. <b>Second entry</b> {220}, right hand on F♯ (<i>poco rinforzando, ma
     non forte</i>). The texture thickens into three voices {224-237}. {238} <i>legatissimo, f cantando</i>: a chromatic descent in bass octaves
     (C♯–C–B–B♭–A♭–G–F♯) against chordal syncopations. {245} <i>sempre al rigore di tempo, pp</i>. Further entries of the subject-head: on B {258} (<i>a tempo,
     addolcito</i>), on A {266}, and in the left hand at the <b>original pitch</b> C–A♭–G–F {276} (<i>f m.s.</i>), the fugue reclaiming the
     tonic. {282} <i>ff tempestoso</i> over a C pedal (the dominant of F), {286} <i>ff</i>, {287} <i>fff</i>, {288} <i>diminuendo, dolente</i>,
     {290} <i>lunga</i>.`],
    [291, 394, 1, "Recapitulation", "Tempo I · concentrando", "F minor → E♭ / B♭ → cadenzas",
     `The recapitulation starts as a literal reprise of the first group, then moves further and further into improvisation. The second group
     comes back a fifth higher than in the exposition, not in the tonic, and <i>quasi cadenza</i> episodes replace the third group.
     The tonic is left for the coda to settle.`],
    [291, 320, 2, "First group, dissolving", "Tempo I · quasi cadenza · 4/4 brioso", "F minor",
     `{291} the {m:threat} at its original pitch, literally. It soon dissolves: {296} <i>poco a poco abbandonamente, quasi cadenza</i>, {299}
     <i>crescendo ed accelerando</i>, a new 4/4 <i>f brioso, con moto</i> {302}, <i>meno mosso rubato</i> {305}, <i>sempre quasi Cadenza</i>
     {309}, <i>poco a poco agitato</i> {311}, <i>con moto tempestoso</i> {317}, and a two-handed sweep up to a <i>lunga</i> fermata {320}.`],
    [321, 345, 2, "Second group, a fifth higher", "a tempo (con entusiasmo) · 9/8", "E♭ → B♭",
     `{321} the 9/8 group returns <i>con entusiasmo</i> with the {m:toll}, now on E♭ rather than A♭. The {m:cantsigh} and {m:arp} follow at {326},
     <i>dolce cantando</i>, a fifth higher than at {59}. {330} <i>f</i>, {332} <i>più dolce</i>, {334} <i>poco a poco dimin.</i>, {340} <i>crescendo</i>, {342}
     <i>rinforz.</i>, and {343} <i>poco a poco ritardando</i> into the cadenzas.`],
    [346, 364, 2, "Cadenza episodes", "Meno mosso · Andantino meditamente", "G / D♭",
     `{346} <i>Meno mosso (sempre quasi Cadenza), legatissimo</i>: chromatic descents in both hands, <i>pp</i>, then <i>ppp languido</i> fermatas {348-349}.
     {350} <i>Andantino meditamente, rubato, sempre quasi una Cadenza (poco languido)</i>: the cantabile idea, now in a G–D♭ tritone frame, with a long
     semiquaver arch below. {352} <i>poco crescendo ed agitato</i>, {356} <i>languido, espressivo</i>, {358} <i>p dolcissimo</i>, {360} <i>molto languido</i>,
     {361} <i>accelerando, leggiero</i>, ending on a fermata {364}.`],
    [365, 394, 2, "Allegro non troppo mobile e risoluto", "ma sempre poco rubato, quasi Cadenza · 3/4", "F minor",
     `{365} the threat's double-dotted rhythm returns, <i>tre corde</i>, as an energetic Allegro, a substitute for the third group. {369} <i>poco a poco
     crescendo ed agitato</i>, {373} <i>ff cantando appassionato</i>: the climax of the recapitulation. {381} <i>poco a poco accelerando e crescendo</i>,
     {383} <i>a tempo</i>, {386-389} <i>con moto egualmente</i>, falling arpeggios, then <i>pp</i> octaves {390-394} that lead straight into the coda.`],
    [395, 448, 1, "Coda", "Sempre animato (al rigore di tempo)", "F minor → F major",
     `The coda takes over the job the recapitulation didn't do: it settles the tonic. It does so in an unexpected way, landing on F <i>major</i>
     approached from B⁷, the tritone.`],
    [395, 433, 2, "Animato", "p marcato · ff", "F minor",
     `{395} <i>Coda, p marcato</i>: the {m:coda}, semiquaver cells sequenced down by semitones over leaping basses. {401} <i>f</i>, {406} <i>p subito, molto cresc.</i>,
     {409} <i>ff</i> with the {m:toll} on F minor⁷, {413} <i>p</i>, {415} <i>f</i>, {418} <i>ff</i>, {420-425} <i>sf</i>-<i>p</i> waves over rising semiquaver basses,
     {423} <i>sempre diminuendo</i>, and {427} <i>pp</i>, {428} <i>p marcato</i>: the semiquavers in the bass, rising toward the Maestoso.`],
    [434, 448, 2, "Maestoso and final cadence", "Maestoso · Molto animato (ma ritmico) · fff", "D♭ → F major",
     `{434} <i>Maestoso, f allargando</i>: D♭ major⁷ (VI). {436} <i>Molto animato (ma ritmico)</i>, one flat: right-hand triplets on A over a
     {m:lament} in the bass (F–E–E♭–D–D♭–C–B–B♭–A), a passus duriusculus spanning a minor sixth. {444} <i>ff</i> fermata on B (B–F tritone),
     {446} <i>poco accelerando</i>. {447} <i>fff</i>: a sweep over B (B⁷ with C♯, D♯ and A), which resolves <i>directly</i> to F major {448}. The final cadence is
     a tritone substitution, and the minor-mode threat ends on a bare F–A third.`],
  ],

  essays: [
    ["Thematic transformation", `
    <p>The whole sonata grows from the <b>{m:threat}</b>. Its components (a falling third, two steps, and the double-dotted rhythm) are
    developed separately:</p>
    <ul>
     <li><b>Pitch, transposed</b>: on E {24}, C♯ (F♯ minor) {79}, A {81}, C/F♯ in the bass {133}, B {174}.</li>
     <li><b>Pitch, chromaticised</b>: F♯–D–C♯–C {198}, the fugue subject {210}, its entries {220}, {258}, {266}, and back to the original
     pitches {276}.</li>
     <li><b>Rhythm alone</b>: the {m:ddrhythm} also shapes the left hand's rising counter-line, so even the accompaniment is thematic.</li>
     <li><b>Lyricised</b>: the {m:cantsigh} {59} keeps the long–short anacrusis but turns the falling third into a rising one.</li>
     <li><b>The first interval alone</b>: the 9/8 theme opens with the threat's falling major third, C–A♭ {54}.</li>
    </ul>
    <p>Alongside these thematic transformations are <b>textural motifs</b>: the {m:toll}, the {m:knock}, and the symmetrical cells
    ({m:octa}, {m:hexa}, {m:coda}). They are harmonic as much as melodic: Medtner uses symmetrical collections to suspend tonality,
    then returns to functional harmony at the cadences.</p>`],
    ["Harmony and tonal plan", `
    <p>F minor is contested by <b>F♯ minor</b>, the semitone above. The threat appears in F♯ minor at {79}, and the fugue is in F♯ minor {210}.
    The exposition's other keys are conventional: A♭ (III) for the 9/8 second group, C minor (v) for the minaccioso third group. The
    recapitulation of the second group comes back a fifth higher (E♭, B♭), so the tonic is not reached through the second group.
    The resolution is left to the coda.</p>
    <p>Symmetrical harmony: octatonic {87}, hexatonic {91}, semitone-sequenced cells {395}, and tritone frames (B–F at {8}, G–D♭ at
    {350}, B⁷ at {447}). The final cadence B⁷ → F {447-448} is a tritone substitution for C⁷ → F, and the music lands on the tonic
    <b>major</b>.</p>
    <p>The chromatic-saturation curve (Compare tab) shows Minacciosa using the twelve pitch classes more evenly than Night Wind
    almost throughout. Its most "tonal" moments are the 9/8 group and the closing F major.</p>`],
    ["Rhythm and metre", `
    <p>Metre changes mark the form: 3/4 for the first group, <b>9/8</b> for the second group (L'istesso tempo: the quaver stays the same,
    so the bar grows by half), 3/4 for the sordamente and third groups, 9/8 again in the development {156} and recapitulation {321}, and
    one 4/4 passage (<i>brioso</i>, {302}).</p>
    <p>The double-dotted rhythm saturates the piece. Against it, the 9/8 sections move in even semiquavers ({m:toll}), and the fugue
    moves in continuous semiquavers. Medtner's expression marks ask for strict tempo exactly where the music is most threatening:
    <i>sempre al rigore di tempo</i> at {87}, {210}, {245}, {395}. The <i>quasi cadenza</i> markings concentrate in the recapitulation.
    The tempo curve shows the recording's large-scale rubato.</p>`],
  ],
},

compare: `
<h3>Two one-movement sonatas, twenty years apart</h3>
<p>Night Wind (1911) and Minacciosa (c. 1929–31) both put a full sonata cycle into one continuous movement, both begin with a motto
that is at once a melodic cell and a rhythm, and both make that motto return at structural joints. The differences are as instructive as the
similarities.</p>

<h3>Structure</h3>
<table class="cmp">
<tr><th></th><th>Night Wind, Op. 25/2</th><th>Minacciosa, Op. 53/2</th></tr>
<tr><td>Length</td><td>723 bars · ~32 min</td><td>448 bars · ~18 min</td></tr>
<tr><td>Opening</td><td>Introduction (Andante) with motto {nw:1} and cyclic theme {nw:2}</td><td>No introduction: the motto <i>is</i> the first theme {mi:1}</td></tr>
<tr><td>Sonata</td><td>Exposition {nw:38}, development {nw:129}, recapitulation {nw:178} in 15/8</td><td>Exposition with three groups {mi:1}, {mi:54}, {mi:91}; development {mi:140}; recapitulation {mi:291}</td></tr>
<tr><td>Contrapuntal centre</td><td>None as such; the Fantasia layers rising and falling scales {nw:293}</td><td>A fugue on the motto {mi:210}</td></tr>
<tr><td>"Second part"</td><td>A complete Fantasia (Part II), {nw:285-645}</td><td>Cadenza episodes inside the recapitulation, {mi:346-394}</td></tr>
<tr><td>Return of opening material</td><td>Introduction a semitone higher {nw:250}; Coda from the Introduction {nw:646}, {nw:717}</td><td>First group literal {mi:291}; second group a fifth higher {mi:321}</td></tr>
<tr><td>Cadenza</td><td><i>quasi cadenza</i> on F⁷ / B♭ (tritone of E) {nw:561}</td><td><i>quasi cadenza</i> episodes {mi:296}, {mi:346}, {mi:350}</td></tr>
<tr><td>Ending</td><td><i>pp perdendosi</i>; E minor + D♯ F♯ A♯ {nw:723}</td><td><i>fff</i>; B⁷ → F major {mi:447-448}</td></tr>
</table>
<p>Proportions in these recordings: Night Wind's Introduction returns at 47% of the duration and its Part II starts at 55%. Minacciosa's recapitulation starts at 61%,
near the golden section, and its coda at 89%. Night Wind's coda starts at 92%.</p>

<h3>Shared ideas</h3>
<ul>
<li><b>A semitone antagonist.</b> In Night Wind, E against F ({nw:13}, {nw:250}) and E♭ ({nw:153}). In Minacciosa, F against F♯ ({mi:79}, the
fugue {mi:210}). Each tonic's main rival is a chromatic neighbour, not the dominant, and each piece puts its principal theme into that key.</li>
<li><b>The tritone as the last obstacle.</b> Night Wind places its quasi-cadenza on B♭, the tritone of E {nw:561}, and folds A♯ into the final chord.
Minacciosa's last chord before the tonic is B⁷, the tritone of F {mi:447}.</li>
<li><b>Repeated-note "knocking".</b> The wind call's repeated note becomes the Tenebroso pairs ({nw:372}, {nw:646}); Minacciosa has the misterioso
C♯ ostinato {mi:182} and the tolling octaves {mi:54}. In both, a repeated pitch is the image of something outside the music: the wind, a
threat, a bell.</li>
<li><b>Perpetual-motion layers.</b> Under the themes run constant quavers or semiquavers: the wind ostinato {nw:38} and sfrenato runs
{nw:285}; the second-group arpeggio {mi:59} and the fugue's semiquavers {mi:214}.</li>
<li><b>Chromatic descent as lament.</b> The Tenebroso's sliding pairs {nw:372}, the fugue's descending bass octaves {mi:238}, and Minacciosa's
final passus duriusculus {mi:436}.</li>
</ul>

<h3>Differences</h3>
<ul>
<li><b>Thematic method.</b> Night Wind generates themes from three cells (call, sigh, turn) and transforms them in character, with long
lyrical paragraphs. Minacciosa transforms one motif's parameters separately (interval, rhythm, register) in a much more economical, almost
serial way.</li>
<li><b>Harmony.</b> Night Wind is modal-diatonic at heart, with chromatic voice-leading; its keys are real keys. Minacciosa uses symmetrical
collections (octatonic {mi:87}, hexatonic {mi:91}, semitone sequences {mi:395}) to suspend key, then cadences functionally. The
chromatic-saturation curves below show the difference.</li>
<li><b>Metre.</b> Night Wind is quintuple: 5/4, then 15/8 as five dotted beats. Minacciosa alternates 3/4 and 9/8, with the bar
expanding by half.</li>
<li><b>Ending.</b> One dissolves (<i>perdendosi</i>, <i>pp</i>); the other affirms (<i>fff</i>, tonic major).</li>
</ul>

<h3>Connections to your repertoire</h3>
<ul>
<li>Your Sonata-Ballade Op. 27 (1912–14) is Night Wind's near-contemporary. It shares the cyclic method and a semitone-inflected harmonic palette,
and Night Wind's Introduction-theme returns work like the Ballade's motto.</li>
<li>Minacciosa's single-cell saturation is close in spirit to the <b>Berg Sonata</b> you played: one movement, every line derived from
the opening gesture, and functional cadences reached through chromatic saturation.</li>
<li>The fugue {mi:210} with a chromatic subject, countersubject in contrary motion and a final entry at the original pitch will feel familiar
from the <b>Art of the Fugue</b> (Contrapuncti 4, 6, 9). Note how Medtner's <i>sempre al rigore di tempo</i> asks for the same
steadiness.</li>
<li>Night Wind's two-part design (a sonata, then a Fantasia that re-reads it) recalls <b>Schumann's Fantasie Op. 17</b>, especially the
<i>Im Legendenton</i> interruption: compare the Largamente intrusions of the Introduction into the development {nw:135}.</li>
<li>The Russian line: Scriabin's Second Sonata (sonata-fantasy, sea and wind), Feinberg 4, Stanchinsky 2 and Myaskovsky 4 all share
Medtner's habit of one-movement density and pedal-point climaxes.</li>
</ul>
`,
};
