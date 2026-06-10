# Effect Engine — Design

Status: **proposal / direction** (not implemented). Open decisions are marked
`DECISION:`. The goal is one clean, scalable effect-handling system for an
effect-based roguelike (Balatro-like), built *before* piling on content. See the
memory note `effect-system-architecture` for the vision this serves.

---

## 1. Purpose & scope

Unify, under a single model, every "thing that changes how the game plays":

- **Slot effects / slot types** — base, Shop, Dark Shop, Event slots.
- **Chips** — objects mounted on board/slots that *modify the interactions over them*
  (e.g. "multiply coins gained here", "if a merge resolves here, destroy the tile").
- **Cards** — run-level passives that *react to events* (e.g. "every time you merge
  two 2s, gain XYZ").
- **Tile tags** — per-tile state/modifiers (today: `bricked`; future: golden, locked…).

Out of scope for now (explicit): score, defeat conditions.

---

## 2. Core model

### 2.1 Two roles (the central distinction)

Every effect is one or both of:

- **Modifier** — runs *during* an interaction, on a **mutable outcome**, and may
  change the result. Substrate: **interaction hooks**. (chips, special slots.)
- **Reactor** — runs *after* something resolved; observes and acts but cannot
  rewrite the past. Substrate: the **per-turn event log** (already built). (cards.)

A single `Effect` may implement both (e.g. a card "all merges give +1 coin" is a
merge-modifier; "when you merge two 2s, gain a chip" is a reactor).

### 2.2 Scopes (where an effect lives)

An effect is attached to exactly one owner; its *scope* decides which interactions
it sees:

| Scope    | Owner            | Sees                                            |
|----------|------------------|-------------------------------------------------|
| Tile     | `Tile`           | interactions involving that tile                |
| Slot     | `Slot`           | interactions resolving on that slot (+ its chips)|
| Board    | `Board`          | all interactions (board-wide chips)             |
| Run      | `GameRun`        | everything (cards)                              |

A **Chip** is just an `Effect` mounted at Slot or Board scope. A **slot type**
(Shop/Dark Shop/Event) is a slot pre-loaded with one or more Slot-scoped effects —
exactly today's "a slot is a shop because it carries a `ShopEffect`" philosophy,
generalized. `DECISION:` confirm chips and slot-types share the `Effect` mechanism
(recommended) rather than being separate hierarchies.

---

## 3. The `Effect` interface

One base class with **default no-op virtuals**, so each effect overrides only the
hooks it needs (matches the existing single-base `SlotEffect` style; the cost of
calling no-op virtuals is negligible at this scale).

```cpp
class Effect {
public:
    virtual ~Effect() = default;
    virtual std::unique_ptr<Effect> clone() const = 0;

    // Persistence across the turn-to-turn board clone (undo stack). Most effects
    // are persistent; transient ones (e.g. "frozen this turn") return false and
    // are dropped by the clone path — declared ONCE here, not per-field.
    virtual bool isPersistent() const { return true; }

    // --- Modifier hooks: act on a MUTABLE outcome, may change the result. ---
    virtual void onMergeResolving(MergeContext&) {}
    virtual void onCoinsResolving(CoinContext&) {}
    virtual void onSpawnResolving(SpawnContext&) {}

    // --- Reactor hooks: observe; act via ctx; cannot alter the past. ---
    virtual void onEvent(const TurnEvent&, EffectContext&) {}
    virtual void onTurnEnd(const TurnLog&, EffectContext&) {}
};
```

`SlotEffect`/`ShopEffect` collapse into this: `ShopEffect` becomes an `Effect`
whose `onMergeResolving` sets `triggered` + requests the shop (it doesn't alter the
outcome — it's a side-effecting reactor that happens to fire at merge time).

`DECISION:` one fat base (above) vs. small capability interfaces
(`IMergeModifier`, `IReactor`, …) the dispatcher keeps as typed lists.
Recommendation: **one base now**; split only if the hook list explodes.

---

## 4. Interaction contexts (the mutable outcomes)

Each interaction builds a small struct that flows through the in-scope modifiers,
then is applied, then recorded as a `TurnEvent`. The struct is the single place an
interaction's result can be changed.

```cpp
struct MergeContext {
    Slot*  slot;            // where the merge resolves
    int    sourceValue;     // pre-merge value of each tile (immutable input)
    int    resultValue;     // mutable: modifiers may change the merged value
    bool   destroyResult = false;   // chip: "destroy the tile on merge here"
    int    coinReward    = 0;       // accumulator: modifiers add; applied via CoinContext
    EffectContext& ctx;
};

struct CoinContext {        // every coin award flows through here
    Slot*  source;          // slot the coins originate over (nullable)
    int    amount;          // mutable: e.g. a chip multiplies it
    EffectContext& ctx;
};

struct SpawnContext {
    Slot*  slot;
    int    value;           // mutable: modifiers may change the spawned value
    EffectContext& ctx;
};
```

This is what the event log alone could **not** give us: chips need to *alter*
`resultValue` / `destroyResult` / `amount` before they're applied. The log records
the *final* outcome afterward.

---

## 5. `EffectContext` (shared mutation façade)

A thin interface effects (and, eventually, items) act through — so nothing pokes
`GameRun`/`Board` internals directly, and every mutation can be intercepted.

```cpp
class EffectContext {
public:
    virtual ~EffectContext() = default;

    // queries
    virtual int             coins() const = 0;
    virtual const TurnLog&  log()   const = 0;
    virtual Board&          board() = 0;

    // mutations (these route THROUGH the pipelines where relevant)
    virtual void addCoins(int base, Slot* source = nullptr) = 0; // builds a CoinContext, runs modifiers, applies, logs
    virtual void destroyTile(Tile*) = 0;
    virtual void spawnTile(/* … */) = 0;
};
```

`GameRun` implements `EffectContext` (or via a small adapter). `addCoins` is the
canonical example of routing a mutation through its modifier pipeline:

1. build `CoinContext{source, base}`,
2. run `onCoinsResolving` over in-scope effects (in order),
3. apply the final `amount` to `coins`,
4. emit a coin `TurnEvent`.

Re-entrancy rule: modifiers **mutate the context**, they do not call `addCoins`
re-entrantly. (Prevents infinite coin loops.)

`DECISION:` migrate `ItemDef::onUse(GameRun&)` to `onUse(EffectContext&)` now or
later. Recommendation: **later** — keep items working on `GameRun&`, converge after
the engine exists.

---

## 6. Dispatch: scoping + ordering

For an interaction at slot `S` involving tiles `A`,`B`:

1. **Gather** in-scope effects: `A`/`B` tile effects → `S` slot effects/chips →
   board chips → run cards (only those overriding the relevant hook).
2. **Run** the modifier hook on each, in a **deterministic order**.
3. **Apply** the resolved outcome.
4. **Emit** the `TurnEvent`(s) into the turn log.
5. Reactors fire later (end-of-turn pass over the log), or inline for run cards
   that also implement the modifier hook.

`DECISION: dispatch order.` It is a balance lever (Balatro = left→right). Proposal:
**by scope (tile → slot → board → run), then by board position (row-major) within a
scope.** Alternative: insertion order. Must be deterministic and documented.

---

## 7. Worked example — a merge, end to end

Setup: slot `S` has a chip *"×2 coins over this slot"*; the run holds a card
*"each merge of two 2s → +5 coins"*. Player merges two 2-tiles on `S`.

1. `Board` resolves the slide; a merge is about to happen on `S`.
2. Build `MergeContext{slot=S, sourceValue=2, resultValue=4, coinReward=0}`.
3. Run `onMergeResolving` in order:
   - tile effects: none.
   - slot/chip: the ×2-coins chip implements `onCoinsResolving`, not merge → skipped here.
   - run card: "two 2s → +5" sets `coinReward += 5`.
4. Apply: set merged tile value `= resultValue (4)`; `destroyResult` false.
5. `ctx.addCoins(coinReward=5, source=S)` → builds `CoinContext{source=S, amount=5}`
   → the ×2 chip's `onCoinsResolving` doubles it to `10` → apply +10 coins → emit coin event.
6. Emit `TurnEvent::tileMerged(4, 2, S.coord, brickBroke)` (final values).

Both roles cooperated: card (reactor-ish modifier) added reward, chip (modifier)
scaled it, log recorded the truth.

---

## 8. Clone & persistence (retires the flag debt)

- All effects implement `clone()`. `Slot`'s copy-ctor already clones slot effects;
  extend the **tile** clone path likewise. Today `Board::copyStateFrom` rebuilds
  tiles *from value only* and hand-copies `bricked` — instead it must **clone the
  tile's persistent effects** and drop transient ones, driven by
  `Effect::isPersistent()`.
- `Tile::bricked` → a persistent tile effect (immovable + merge-target-breaks).
  `Tile::frozenThisTurn` → a transient tile effect (`isPersistent()==false`), so it
  is simply absent from the next clone — no per-field special-case, no manual
  copyStateFrom edit per new tile type. **This is the lowest-risk first slice and
  the model's proof on a real case.**
- The per-turn **event log stays off the board** (owned by `Turn`, never cloned) —
  unchanged; it remains the reactor feed.

---

## 9. Cards (run-level reactors) & the log

- Cards are run-scoped `Effect`s held on `GameRun` (a `std::vector<std::unique_ptr<Effect>>`).
- Reactor dispatch point: **end of turn**, iterate `currentTurnLog().events()` and
  call `card.onEvent(e, ctx)` (and/or one `onTurnEnd(log, ctx)` per card). This is
  the consumer the event log was built for — replacing the debug dump in
  `Turn::endTurn`.
- A card that is *also* a modifier (e.g. "all merges +1 coin") is included in the
  run scope of the merge dispatch (section 6), so it participates live.

---

## 10. Registries & content

Mirror `ItemRegistry`: content = data + lambdas keyed by id, in core; config-driven
later. Likely: `ChipRegistry`, `CardRegistry`, and a `SlotTypeRegistry`/factory
(Shop/Dark Shop/Event = recipes that attach the right slot effects). Mechanism in
core, policy in registries — no new control-flow per content item.

---

## 11. Migration plan (incremental, behavior-preserving, always builds)

1. **`Effect` base + `EffectContext`**; adapt `SlotEffect`/`ShopEffect` onto them.
   Behavior identical. (Foundations.)
2. **Merge interaction pipeline** in `Tile::mergeIntoSlot`/`Board`: gather scoped
   effects, run `onMergeResolving`, apply, emit the (now post-modifier) event.
   `ShopEffect` plugs in unchanged in behavior.
3. **Tile-tag system**: brick/frozen → persistent/transient tile effects; switch the
   tile clone path to `isPersistent()`. Removes the `copyStateFrom` special-case.
4. **Coin pipeline + `EffectContext::addCoins`**; first **chip** ("×coins over slot").
5. **Chip mounting**: a chip is an `Effect` the player mounts/unmounts on a slot/board;
   first "destroy-on-merge" chip.
6. **Cards/reactor pass** over the log; first card; retire the debug dump.
7. **Registries** for chips/cards/slot-types; config-driven data later.

Cross-cutting derisking: **seeded run RNG** (today seeded once from
`random_device` — non-reproducible) and a first **clone/undo invariant test**
harness (none exist; this engine adds clone-sensitive state).

---

## 12. Decisions (RESOLVED 2026-06-10)

- **Chips and slot-types share the `Effect` mechanism.** A chip = an `Effect`
  mounted at slot/board scope; a slot-type = a slot pre-loaded with effects.
- **One fat `Effect` base** with default no-op hooks (not split capability interfaces).
- **Dispatch order: by scope (tile→slot→board→run), then board position.**
- **`ItemDef::onUse` migrates to `EffectContext` later** (items stay on `GameRun&` for now).
- **Chips are player-mountable AND movable** — bought in shop or loot, then mounted/
  moved by the player via an additional graphical slot-selection + mounting layer
  (UI, designed when chips land). The engine must support runtime mount / unmount /
  move of a slot/board-scoped `Effect`.
