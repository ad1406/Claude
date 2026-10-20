"""From score markup (repeat signs, endings, D.C./D.S., Fine, Coda) to a graph of blocks and allowed moves.

A recording's form is then *found* by the alignment (a path through this graph), never picked from a preset list:
repeats taken or not, endings, D.C. with or without repeats, cuts, jumping to the next movement.

Markup per bar (all optional): repeat_start, repeat_end, ending (1, 2, ...), segno, coda, fine, dc, ds, to_coda,
repeat_to (on a repeat_end bar: another bar the repeat may go back to, e.g. Chopin op. 35/i: m.5 as printed, or m.1).
"""
COST = {"next": 0.0, "ending": 0.0, "repeat": 1.5, "repeat_alt": 2.0, "dc": 0.0, "ds": 0.0, "coda": 0.5}

def blocks_for_movement(bar_ids, mark):
    """bar_ids: notated order; mark: {bar_id: {...markup...}} -> (blocks, edges, start, finals)"""
    m = lambda b, k: bool(mark.get(b, {}).get(k))
    end_of = lambda b: mark.get(b, {}).get("ending")
    alt = {v.get("repeat_to") for v in mark.values() if v.get("repeat_to")}
    cuts = []
    for i, b in enumerate(bar_ids):
        if i == 0: continue
        p = bar_ids[i - 1]
        if (m(b, "repeat_start") or b in alt or m(b, "segno") or m(b, "coda") or end_of(b) != end_of(p)
                or m(p, "repeat_end") or m(p, "fine") or m(p, "dc") or m(p, "ds") or m(p, "to_coda")):
            cuts.append(i)
    idx = [0] + cuts + [len(bar_ids)]
    blocks = [{"id": f"b{k}", "bars": bar_ids[idx[k]:idx[k + 1]]} for k in range(len(idx) - 1)]
    B = {bl["id"]: bl for bl in blocks}
    first = lambda bl: bl["bars"][0]; last = lambda bl: bl["bars"][-1]
    edges = []
    def add(a, b, kind): edges.append((a["id"], b["id"], kind))
    for k, bl in enumerate(blocks):
        nxt = blocks[k + 1] if k + 1 < len(blocks) else None
        e_here = end_of(first(bl))
        # forward: normally the next block; out of a 1st ending you don't walk into the 2nd ending
        if nxt is not None:
            if e_here and end_of(first(nxt)) and end_of(first(nxt)) != e_here:
                pass
            else:
                add(bl, nxt, "next")
        # skipping endings: from the block before a group of endings to each later ending, and from
        # a non-final ending to the block after the whole group (some editions / performers)
        if nxt is not None and end_of(first(nxt)) and not e_here:
            j = k + 1; group = []
            while j < len(blocks) and end_of(first(blocks[j])): group.append(blocks[j]); j += 1
            for g in group[1:]: add(bl, g, "ending")
        # repeat back
        if m(last(bl), "repeat_end"):
            j = k
            # the matching start: nearest earlier block that begins with repeat_start (not an ending), else movement start
            tgt = blocks[0]
            for jj in range(k, -1, -1):
                bb = blocks[jj]
                if m(first(bb), "repeat_start") and not end_of(first(bb)): tgt = bb; break
            add(bl, tgt, "repeat")
            to = mark.get(last(bl), {}).get("repeat_to")
            alt_bl = next((bb for bb in blocks if first(bb) == to), None) if to else None
            if alt_bl is not None and alt_bl is not tgt: add(bl, alt_bl, "repeat_alt")
        if m(last(bl), "dc"): add(bl, blocks[0], "dc")
        if m(last(bl), "ds"):
            seg = next((bb for bb in blocks if m(first(bb), "segno")), blocks[0]); add(bl, seg, "ds")
        if m(last(bl), "to_coda"):
            cod = next((bb for bb in blocks if m(first(bb), "coda")), None)
            if cod: add(bl, cod, "coda")
    finals = [bl["id"] for bl in blocks if m(last(bl), "fine")]
    if blocks: finals.append(blocks[-1]["id"])
    return blocks, edges, blocks[0]["id"] if blocks else None, sorted(set(finals), key=lambda x: int(x[1:]))
