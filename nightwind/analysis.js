// Walkthrough text. Markup: {12} or {12-15} bar links, {m:call} motif chips, {f:song} family chips.
window.NW_TEXT = {

thesis: `
<p>On first hearing the Night Wind can seem a chain of episodes: a slow introduction, a sonata Allegro, a slow return, a
<i>sfrenato</i> Fantasia, a Tenebroso, cadenzas, a coda. Underneath, it is built from three ideas stated in its first four bars,
and almost every bar afterwards is one of them, two of them at once, or a transformation of one into another.</p>
<ul>
<li>{f:call}: the falling third with its lower note struck again ({1}). It marks the <b>formal joints</b>: the start of the
Introduction, of the Allegro ({38}), of the recapitulation ({179}), the interruptions of the development ({135}, {139}), the turn
into F minor ({249}), the cadenza of Part II ({561}) and the Largamente ({629}).</li>
<li>{f:song}: the tranquillo theme ({2-4}), made of a <i>sigh</i> (a held note and its lower neighbour), a <i>climb</i> (two
steps up, one back) and a <i>swinging fourth</i>. Its rising motion is the material of every lyrical and every driving
passage: the Allegro's themes, the Part II runs ({285}), the meditations ({442}).</li>
<li>{f:descent}: the line that falls against the Song in bar 2 (E–D–C♯–C). It is the bass of the Introduction, the top voice
of the Stentato ({107}), the off-beat pairs of the Tenebroso ({372}), the structural bass of the Vertiginoso ({496}) and the
last thing heard before the final ascent.</li>
</ul>
<p>The Song and the Descent are a pair: they almost always sound <b>together, in contrary motion</b> (the "wedge": select a statement and press W to see its partner, drawn dashed). The piece's large-scale drama is the balance between them. In Part I the Song leads and the Call
articulates the form. In Part II the Call almost disappears, the Descent grows (it dominates the Tenebroso, the Vertiginoso
and the Coda) and the Song climbs through new keys (E, F, F♯, B, C) until it returns to E, at the very end in the bass,
and is carried up and away into silence.</p>`,

stations: [
  // ---------------- PART I
  [1, 1, "Introduzione · the Call", "Andante ♩ = 50 · ff all'improvisa",
   `The {m:call}: G–E–E in triplets, as chords, thrown down through the registers (G6 → G5 → G4, with the left hand
   answering on G3). Everything here is the "loud" voice of the wind: a falling third, a re-struck note. Hear how the left hand
   keeps repeating the figure after the right hand has stopped. The call does not end; it turns into the accompaniment of the
   next bar.`],
  [2, 8, "The Song and the Descent", "4/4 · p tranquillo",
   `The {m:songfull} enters in the top voice: E held, the {m:sigh} (E–D♯–E–F♯), then the {m:climb} G–A–F♯–G–A–B and the
   {m:swing} B–E–B–E ({4}). Look underneath. Each beat of the left hand is still a {m:call} (a chord, then the lower note
   twice), and the lowest notes trace the {m:counter} E–D–C♯–C ({2}), continuing B–A–G–F♯ ({3}): the {f:descent}. The Song
   rises while the Descent falls. This contrary-motion "wedge" is the sonata's basic texture. At {6} the whole phrase is
   restated a fourth lower (B–A♯–B–C…), the swing on F♯–B ({8}).`],
  [9, 12, "First intensification", "",
   `The phrase breaks into sequences: wide leaps in the right hand ({9}), the {m:call} rhythm still pulsing in the left hand.
   The inner voices now move in {m:chrom} lines ({10-11}), and at {12} the Descent reaches the melody itself: C–B–A♯–A
   over a bass of D♯ and E, the semitone below the tonic.`],
  [13, 21, "The F-major shadow; cantabile", "ten. · poco calando · cantabile dolce",
   `Over an F bass ({13}) one sextuplet line does both of the Song's motions at once: it climbs in steps with turns
   (C–D–E–F–E–F–G–G♯…), then pours down chromatically (D–C♯–C–B–A♯–A). F, the semitone above the tonic, is the first appearance
   of the key in which the whole Introduction will return at {250}. <i>Cantabile dolce</i> ({14}): a held B with its upper neighbour (B–C–B–A, the sigh inverted); the tenor keeps the call's
   echo pairs (B, then D–D: the third rising), and the bass climbs G–G♯–A–B, the Song's ascent; <i>legatissimo, espressivo</i> ({16}) and the <i>agitato</i> surge ({19-21}) pile up
   sighs and swings.`],
  [22, 33, "Climax of the Introduction", "ff · appassionato",
   `A <i>ff</i> statement of the {m:songfull} on E ({22}) over a C pedal, the Descent in the inner voices. Then <i>appassionato</i>
   ({25}): the swings and climbs in octaves, the bass falling in steps. The Introduction reaches its peak on the Song, not on the Call.`],
  [34, 37, "Poco a poco più mosso", "5/4 · p",
   `The left hand now plays the Allegro's ostinato before the Allegro has begun: C–A–E–F♯–G–A–G–F♯–E–D–C–D–B–C–A ({36}),
   the {m:ostinato} in its Introduction form. It is a swinging fourth opened out into a scale that climbs and descends: Song and
   Descent folded into one line. The right hand's sextuplets accelerate into the Allegro.`],

  [38, 53, "Allegro · first group", "15/8 · ♩. = 92–100 · fz · pesante",
   `The {m:call} again, at its original pitch (G–E–E), now in dotted crotchets: the triplet of bar 1 <b>augmented</b> into the
   compound metre. 15/8 is five dotted beats, so the Introduction's 5/4 survives at a faster tempo. Under it the
   {m:ostinato} (E–B–E–F♯–G–A–G–F♯–E–D–C–D–B–C–A) never stops. The <i>pesante</i> theme ({40}) starts from the call's G–E and
   rises through E–G–B (the third turned upward); <i>cantando</i> ({42}) is a {f:song} climb with re-struck notes
   (G–A–B–B–B–C–D–E) that falls back by step. The first group is the Call and the Song spoken in one breath.`],
  [54, 71, "Risoluto · leggiero vivo", "f risoluto · fz · leggiero",
   `<i>Risoluto</i> ({54}): a descending scale in octaves (E–D–C–B–A–A–B–G♯), the {f:descent} in the top voice over arpeggiated
   bass. <i>Leggiero, vivo</i> ({55}) breaks the material into running semiquavers; <i>ffz</i> ({59}) and the long <i>leggiero</i>
   paragraph ({62-71}) keep the climbs and swings in circulation. {m:chrom} lines in the inner voices ({61}, {67}) keep the
   counterline alive.`],
  [72, 79, "Transition: molto pesante", "mp → fff → tranquillo",
   `The bass climbs <b>chromatically</b> A♯–B–C–C♯–D in dotted crotchets ({72}): the {m:chrom} descent turned upside down, while
   the inner voices hammer the D–G♯ tritone. At the <i>fff</i> ({76}) the bass is still climbing by semitone (E–F–F♯) under
   repeated A♯ chords, the {f:call}'s re-struck note blown up into an ostinato. <i>Sempre più tranquillo</i> ({77}), and at {79}
   the call passes softly between the hands (A–F♯–F♯) to open the second group.`],
  [80, 95, "Second group", "Tranquillo ♩. = 72–80 · D major · pp",
   `The second theme is an {m:arch}: F♯–A–C♯–F♯–C♯–A, an arpeggio rising and falling, in dotted crotchets ({80}). Compare
   the Call. It is a chain of falling thirds played upward and back. Its continuation (F♯–B–B–B–A, {82}) re-strikes a note
   like the Call and resolves with a {f:song} rise (E–F♯–G, {83}). <i>Poco a poco scherzando</i> ({85}): the arch in dotted rhythms;
   a {m:sigh} in the bass ({89}); <i>scherzando</i> exchanges between the hands ({92}); <i>svegliando, veloce</i> ({94}).`],
  [96, 107, "Giocondamente", "♩. = 88–92 · leggiero · sdegnoso · risoluto, pesante",
   `The {m:pedal}: A–E–D–E–D–E, a melody tone followed by the {m:sigh}'s neighbour note in perpetual motion. The tune is
   underneath, in the tenor (C♯–B–C♯–D, the sigh again, {96}) and then on top ({97}: A–G♯–F♯–G♯–A–B–C♯, a {f:song} rise after
   a neighbour). Wind and Song have become one texture. <i>Sdegnoso</i> ({104}) and <i>risoluto, pesante</i> ({106}) darken it with
   B♭ and A♭ chords. Then {107}: <i>ten., calando</i>, the top voice falls A–G♯–G–F♯–F–E, the {m:chrom} {f:descent} exactly, doubled in
   the tenor.`],
  [108, 128, "Stentato · closing group", "♩. = 72 → Giocondamente ♩. = 92 · ff risoluto",
   `<i>Stentato</i> ("labouring"). Above, the descent of {107} has just finished; below, the bass moves in dotted crotchets
   F♯–A–D–F♯–D | A–F♯–A–D–A, the {m:arch} of the second theme augmented ({108}). Between them, arpeggio waves, one per
   beat. So the second theme (arch) and the counterline (descent) are combined at the moment the music is most like
   wind. <i>Poco a poco animato</i> ({110}) to <i>ff appass.</i> ({116}) and the <i>Giocondamente ff risoluto</i> ({118}), where the left hand's
   chords circle F♯–E–F♯–G (the {m:circle}) under pentatonic waves. Ten-note rushes ({125}, {127}), then <i>molto calando</i> ({128}).`],
  [129, 152, "Development: the Call interrupts", "ppp dolce · Largamente ♩. = 60 · fz all'improvisa",
   `The development starts in F♯ minor ({129}) with giocondamente figures. Then the Introduction breaks in, <i>Largamente,
   all'improvisa</i>: the {m:call} on F♯–D♯–D♯ over a B-major chord ({135}), and on A–F♯–F♯ ({139}), each time falling through the
   octaves as in bar 1. Between them, arpeggio waves ({136}) and a <i>molto tranquillo</i> chorale whose top voice rises in an
   arch F♯–A♯–C♯–F♯ ({137}). The development has the largest number of calls in the sonata ({144-149}, <i>più mosso</i>, call
   fragments passed from hand to hand).`],
  [153, 165, "E♭ minor: cantabile, placido", "six flats · pp",
   `The far point: E♭ minor ({153}), the semitone below the tonic, as F was the semitone above. The melody begins with the Call's falling third
   <b>augmented</b> (A♯–F♯, the F♯ held, {154}), over a {m:pedal}-like fifth oscillation (B♭–E♭) and a bass that
   climbs in scales. <i>Poco a poco ritornando al tempo I</i> ({156}); bass scales rising ({160}, F–G–A–B♭–C–C♯), <i>risoluto</i>
   ({162}), <i>addolcito, espressivo</i> ({163}).`],
  [166, 178, "Retransition", "f · risonante · leggierissimo · non legato",
   `The key signature clears ({166}); <i>ff risonante</i> ({169}) and <i>leggierissimo</i> ({170}) sweep the register; the E-minor
   signature returns ({173}) and <i>non legato</i>, <i>risoluto</i> runs ({174-177}) set up the recapitulation.`],
  [179, 203, "Recapitulation · first group", "Tempo I · ff · molto pesante",
   `{m:call} and {m:ostinato} at full strength ({179}), the pesante theme <i>molto pesante</i> ({181}). The first group is compressed;
   the {m:ostinato} returns in all its forms ({187-190}) and <i>f risonante</i> ({199}) leads, <i>sempre più tranquillo e dim.</i>,
   into the second group.`],
  [204, 233, "Recapitulation · second and closing groups", "Tranquillo ♩. = 80 · G major · stentato",
   `The {m:arch} theme in G ({204}), the relative major the exposition avoided. Then the {m:pedal} ({210}) and the
   Stentato waves ({222-229}), now <i>sempre più agitato</i>, growing to <i>ff appassionato</i> ({230}) over B major arpeggios.`],
  [234, 249, "E major: a false victory", "four sharps · giocondamente · con strepito",
   `The closing group arrives in the tonic <b>major</b> ({234}): <i>giocondamente</i>, <i>Molto giocondamente</i> ({241}) with rising E-major
   arpeggios while an inner voice falls chromatically (D♯–D–C♯–C, {241}, the {f:descent} again), <i>ff pesante</i> ({243}), <i>con
   strepito</i> octave leaps ({246}). Then the {m:call} enters on A♭–F ({248-249}), and the music drops a semitone, into F minor.`],

  [250, 261, "Return of the Introduction, in F minor", "Tempo dell'introduzione · 4/4 · p",
   `The {m:songfull} note for note, a semitone higher (F–E–F–G | A♭–B♭–G–A♭–B♭–C), with the same accompaniment: calls on
   every beat, the {m:counter} F–E♭–D–C♯ below. The shadow key of {13} has become real. At {254} the Song is restated on D♯,
   a semitone <i>below</i> the original, so the two neighbours of E (F and E♭) are both used for the Song.`],
  [262, 284, "E minor again; the climax rebuilt", "teneramente · agitato · ff · molto allargando · Attacca",
   `<i>Teneramente</i> ({262}) back in E. <i>Agitato</i> ({267}), <i>ff</i> ({270}), and the {m:songfull} <i>cantando</i> on E5
   ({271}), an octave above bar 2. A 3/2 bar ({272}), <i>molto allargando</i>, then a <i>ff risonante</i> trill on D♯ over a B pedal, with
   F♯ and F a semitone apart in the left hand ({276}): the dominant with the semitone conflict inside it, <i>perdendosi, lento</i>. The Song is tried on F and G♯ ({278-279},
   <i>sempre accelerando</i>), <i>veloce</i> ({281}), <i>leggierissimo</i> ({283}), <i>lunga</i>, <i>Attacca</i> ({284}).`],

  // ---------------- PART II
  [285, 292, "Part II · Allegro molto sfrenatamente", "𝅗𝅥 = 84 · mp marcato",
   `The Fantasia's theme is the Song <b>diminished</b>: D♯–E–F♯ (the sigh's neighbour, now a pickup), then G–A–F♯–G–A–B,
   literally the {m:climb} of bar 3 in semiquavers, chained (G–A–B–C–B–A–B…) into a run. The answering bar ({286}) comes
   down the same way (E–B–A–G–F♯–E–D♯–D–C♯, the climb inverted, ending chromatic). The left hand's chords descend E–D♯–D–C♯–C–B–A,
   the {m:chrom} counterline of bar 2. Part II begins as the Introduction did: Song above, Descent below, but four times faster.`],
  [293, 310, "Chorale over climbs", "mp · crescendo · ff",
   `The wedge with the parts exchanged: the right hand sings a {m:steps} chorale F♯–G (G G G)–F♯–E–D–C–B–A–G, the {f:descent} augmented
   and with re-struck notes (the {f:call}'s repetition), while the left hand runs the {m:climb} upward (B–G–A–B–G–A–B–C–A–B–C–D,
   {293-294}). It is repeated in sequence ({297-300}), <i>f</i> ({302}), <i>ff</i> ({304}).`],
  [311, 326, "Tumultuoso", "p → ff tumultuoso · pesante",
   `Triplet basses, the runs in both hands, building to <i>ff tumultuoso</i> ({324}) and <i>pesante</i> ({325}).`],
  [327, 335, "Leggiero, espressivo, carezzando", "8 · p leggiero",
   `The texture thins. Dyads falling by tritone and third ({327}), an <i>espressivo</i> line ({328}), <i>carezzando</i> ({330}). In the
   left hand, re-struck notes falling by step and third: {m:hammer} figures ({332-335}).`],
  [336, 361, "Susurrando", "pp a tempo · susurrando",
   `"Whispering": the left hand circles B–C–B–A–B–C–B–A, the {m:circle} (the {m:sigh}'s neighbour note doubled into a turning
   figure), while the right hand repeats B–B–B and falls by step (B–A–G–A–G–F♯…), a {f:call} repetition becoming a
   {f:descent}. The circling runs through most of the next twenty-five bars ({336-361}).`],
  [362, 371, "The Song in F♯", "cantando · f allargando · D major leggiero",
   `The {m:songfull} <i>cantando</i> on F♯ ({362}: F♯–F–F♯–G♯ | A♯–B–G♯–A♯–B–C♯), a tone above the original. With the F-minor
   return ({250}) the Song has now been heard a semitone and a whole tone above home. <i>F allargando</i> ({365}), then D major
   <i>leggiero</i> ({368}) with the circling figure in both hands.`],
  [372, 391, "Tenebroso", "𝅗𝅥 = 60–66 · p poco pedale",
   `The emotional centre. Off-beat pairs B–B, B–B, B–B, A♯–A♯, A–A, G♯–G♯, G–G, F♯–F♯: the {m:chrom} {f:descent}, every tone
   re-struck. That is exactly the accompaniment rhythm of bar 2 (a beat, then two echoes), slowed and given the counterline's
   pitches. Below, the bass rises in arpeggios (B–D–G–D, F♯–D–E–C♯), so the wedge is still there, now dark. {374}: the pairs
   rise (B–C♯–D–F♯), {380}: F♯ pairs again, falling.`],
  [392, 411, "Meno mosso, una corda", "pp senza pedale · sempre crescendo ed agitato · ff",
   `The Tenebroso pairs an octave higher, E–E–E–D♯–D–C♯–C–B ({392-393}), over rising arpeggio basses: {m:chrom} again. At
   <i>sempre crescendo ed agitato</i> ({396}) the pairs turn round and climb (E–F♯–G–A), the Descent inverted, with <i>fz</i>
   accents ({397-399}), to <i>ff</i> ({408}).`],
  [412, 441, "Espressivo, dolente", "p/mf espressivo, dolente · risoluto · pleno",
   `B (held)–A♯–A–G♯ ({412}): the Descent in long notes, under a high B. <i>Risoluto</i> ({418}), <i>espressivo</i> ({424}); the
   left hand circles ({420-433}). <i>F pleno</i> ({436}): B held, A♯, A, over a bass in octave leaps (B–D–G–E).
   <i>Poco a poco ritenuto</i> ({438}), <i>diminuendo</i> ({440}).`],
  [442, 479, "Con meditazione", "slentando, cantabile · Meno mosso · svegliando · con strepito",
   `The {m:songfull} in B minor ({442}): B (held)–A♯–B–C♯ | D–E–C♯–D–E–F♯–E–D–E, then an octave lower ({444-445}). The key
   signature clears ({450}) and the Song appears in C, in the bass. <i>Poco a poco crescendo e svegliando</i> ({454}), <i>f</i> ({458}),
   <i>a tempo</i> ({462}), and a long crescendo ({471}) to <i>f con strepito</i> ({478}). The Descent dominates the structural lines
   throughout.`],
  [480, 495, "Five sharps · marcato il basso", "ff · p marcato il basso",
   `G♯–G♯–G–F♯–F–E–D♯ ({480}), F♯–F♯–F–E–D♯–D–C♯ ({481}): a re-struck note then a {m:chrom} descent, the Call's repetition and
   the counterline joined into one gesture, over a D♯ pedal. <i>Marcato il basso</i> ({484}).`],
  [496, 533, "Vertiginoso", "p leggiero → ff · molto vertiginoso",
   `The top voice repeats the gesture (A–A–G♯–G–F♯–F–E, {496}). The bass does the same at a deeper level: its structural tones
   A–A–A–G♯ | G–F♯–F–E fall chromatically one per beat, and each is decorated by its lower neighbour an octave higher
   (G♯–A, F♯–G…). So the {m:chrom} {f:descent} carries the {m:sigh}'s neighbour on every tone. This is the most "dizzying"
   (<i>vertiginoso</i>) passage, and the one with the most Descent statements in the whole sonata. <i>Molto vertiginoso</i> ({522}),
   <i>pp leggiero</i> ({526}), where the bass of {528} is the counterline cell of bar 2 itself (E–D–C♯–C on the beats) while
   the right hand rises E–F♯–G♯–A. To <i>ff tumultuoso</i> ({532}).`],
  [534, 560, "Tempo I: the sfrenato returns", "molto marcato · f · poco allarg.",
   `The Fantasia's theme at its original pitch ({534}): {m:climb} runs over a chromatically descending chord bass
   (E–D♯–D–C♯…), with the hands in new registers. The chorale-over-climbs follows ({542-548}) as at {293}.`],
  [561, 576, "Quasi cadenza", "fff · Pedale tenuto · calmando · carezzando · susurrando",
   `<i>Fff, quasi cadenza (fermato)</i>, the pedal held through the whole passage ({561-564}): A♯–A♯–A♯–A♯–A–F♯–A, G–E–G. The
   {m:hammer}: the Call's re-struck note moved to the front and multiplied, its falling third at the end. Under an F pedal
   this is the dominant of B♭, the tritone of E, the farthest point of the sonata. <i>Calmando</i>, <i>carezzando</i>,
   <i>leggierissimo</i> ({563-567}); <i>Tempo I, susurrando</i> ({571}), the left hand circling D–D♯–D–C (the {m:circle}) under a held G.`],
  [577, 615, "Concentrando: the Song climbs", "ma sempre con moto · cantabile · agitato · ff molto agitato",
   `The {m:songfull} <i>cantabile</i> in C ({579}), then on D♯ ({585}), G ({595}) and C♯ ({599}), a sequence of statements
   rising through the circle of keys while the music becomes <i>agitato</i> ({601}) and <i>ff molto agitato</i> ({604}: E held, then
   D♯–D–C♯–D♯–C♯–C–A♯–C–A♯–A, a descent built of sighs).`],
  [616, 628, "The Song at home, affannato", "meno mosso · accelerando · ff affannato",
   `The {m:songfull} on <b>E</b> at its original pitch ({616}: E–D♯–E–F♯ | G–A–F♯–G–A–B), for the first time since the
   Introduction. Each statement is cut off by a <i>ff affannato</i> ("breathless") outburst whose top voice is a {m:hammer}
   (D–D–D–D–C♯–A♯, {618}). Then again a third higher ({620}).`],
  [629, 646, "Largamente: the Call returns", "Largamente · plenissimo · poco a poco più mosso",
   `<i>Largamente</i> ({629}): G–E, G–E falling through the octaves, the {f:call} of bar 1 again, now with the repeated note in the
   inner voice. <i>Plenissimo</i> ({631}): the {m:songfull} on E5 over the full texture. <i>Poco a poco più mosso</i> ({633}), <i>sempre
   più dolce</i> ({634}), calls in the left hand ({639-645}), <i>diminuendo</i>.`],
  [647, 684, "Coda: Tenebroso", "𝅗𝅥 = 60 · p · acciacato · dolente",
   `The Tenebroso returns a tone lower: A–A, A–A, A–A, G♯–G♯, G–G, F♯–F♯, F–F, E–E ({647-648}) over rising arpeggios. The coda
   is dominated by the {f:descent}: it has more Descent statements than any other section except the Vertiginoso. <i>Acciacato</i>
   ({667}: crushed octaves in the bass, each tone approached from the semitone below (D♯–E, F♯–G, A♯–B), under a re-struck B whose
   inner voice falls G–F♯–F), <i>dolente</i> ({673}: E held–D♯–D–C♯ over a bass
   C–C–C–B | A♯–A–G♯–G, two {f:descent}s at once), <i>fz</i> hammer-strokes ({679-684}).`],
  [685, 716, "Sempre più sfrenamente, al fine", "ff → p · sempre dimin. al fine",
   `E held, D♯, D ({685}): the descent again, over a bass in octave leaps (E–G–C–A). From here
   to the end the music only diminishes ({686} <i>al fine</i>, {709} <i>sempre dimin. al fine</i>). The {m:songfull} appears once
   more on E ({697}).`],
  [717, 723, "Tenebroso, legatissimo · perdendosi", "pp",
   `The last statement of the {m:songfull}, in the bass, on E2: E–D♯–E–F♯–G–A–F♯–G–A–B, the Introduction theme in semiquavers.
   It continues rising (B–E–B–E, the {m:swing}; then arpeggios) through four octaves to B6 ({717-721}), <i>perdendosi</i>. A last
   <i>leggierissimo</i> sweep ({722}) and the final chord ({723}): E–G–B with D♯, F♯ and A♯, the tonic with its lower neighbour
   (the sigh) and its tritone.`],
],
};
