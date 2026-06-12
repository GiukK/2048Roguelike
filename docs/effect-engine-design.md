# Effect Engine — Design

Status: **in progress** — slices 1–4 and 6 of §11 are implemented (scope-agnostic
`Effect` base, merge interaction pipeline, tile tags + capability queries,
coin pipeline + first chips, cards/reactor pass + `EffectContext`). Remaining:
chip mounting (§11.5) and the content registries (§11.7). Open decisions
are marked `DECISION:`; resolved ones live in §12–§13. The goal is one clean,
scalable effect-handling system for an effect-based roguelike (Balatro-like),
built *before* piling on content. See the memory note `effect-system-architecture`
for the vision this serves.

---

## 1. Purpose & scope

Unify, under a single model, every "thing that changes how the game plays":

- **Slot effects / slot types** — base, Shop, Dark Shop, Event slots.
- **Chips** — objects mounted on board/slots that *modify the interactions over them*
  (e.g. "multiply coins gained here", "if a merge resolves here, destroy the tile").
- **Cards** — run-level passives that *react to events* (e.g. "every time you merge
  two 2s, gain XYZ").
- **Tile tags** — per-tile state/modifiers (today: `bricked`; future: golden, locked…).

Out of scope here: score. Defeat conditions, enemies and bosses are designed in
`docs/boss-design.md` (2026-06-12), which builds on this engine and extends §13's
undo semantics to boss fights.

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

    // --- Post-apply notify (ADDED 2026-06-12): fires AFTER the outcome has
    // been applied and its event logged; context is const. Site side effects
    // that EMIT events belong here, so their events follow their cause in the
    // log. Future interaction pipelines (AttackContext) mirror this
    // pre-apply-modify / post-apply-notify shape.
    virtual void onMergeApplied(const MergeContext&) {}

    // --- Reactor hooks: observe; act via ctx; cannot alter the past. ---
    virtual void onEvent(const TurnEvent&, EffectContext&) {}
    virtual void onTurnEnd(const TurnLog&, EffectContext&) {}
};
```

`SlotEffect`/`ShopEffect` collapse into this: `ShopEffect` becomes an `Effect`
whose merge hook sets `triggered` + requests the shop (it doesn't alter the
outcome — it's a side-effecting reactor that happens to fire at merge time).
SINCE 2026-06-12 it lives on `onMergeApplied`, not `onMergeResolving`: firing
post-apply puts its `ShopTriggered` event AFTER the `TileMerged` that caused it
(cause before consequence; pinned by an ordering test in the suite). Dispatch:
`Slot::resolveMerge` runs the pre-apply leg, `Slot::notifyMergeApplied` the
post-apply leg (called from `Tile::mergeIntoSlot` after the event push, before
the coin-reward routing).

`DECISION:` one fat base (above) vs. small capability interfaces
(`IMergeModifier`, `IReactor`, …) the dispatcher keeps as typed lists.
Recommendation: **one base now**; split only if the hook list explodes.

**Capability queries (IMPLEMENTED, slice 3).** Besides the hooks, the base carries
declarative capabilities — an effect states what its *presence* does to its owner,
and owners aggregate them so board logic never type-switches on concrete effects:

- `immobilizesOwner()` — owner can't slide nor initiate merges (brick, frozen).
  Aggregated by `Tile::isImmobilized()`, checked in `Board::resolveNextTileMove`.
- `protectsOwner()` — owner is off-limits to destroy/wrench/shuffle/area targeting
  (the shop). Aggregated by `Slot::isProtected()`.
- **Run-scope capabilities (2026-06-11)** — same pattern, aggregated by GameRun
  over the owned cards; used for COMMUTATIVE run modifiers (product/sum), where
  a context pipeline's ordering guarantee would buy nothing:
  `spawnCountFactor()` (default 1; Vase of Two = 2, copies multiply, 0 = "no
  spawns", capped at 64 — read by `Turn` BoardResolution via
  `GameRun::getSpawnCountPerTurn()`) and `hourglassRewinds()` (default 0; Back
  to Back = 3; copies SUM their full value and REPLACE the base single rewind:
  none = 1, one = 3, two = 6 — read by the Hourglass via
  `GameRun::getRewindDepth()`, which rewinds up to that many turns, clamping at
  the first; consumed iff at least one rewind happened). RULE OF THUMB: outcome
  needs ordering or carries data → context pipeline; commutative scalar →
  capability.
- Presentation hints: `slotTextureId()` (slot skin) and `overlayTextureId()`
  (marker drawn over the owning tile — brick/frozen show the brick marker).

Typed lookups stay legal for *lifecycle* code that genuinely needs one concrete
effect's state (the shop's `triggered`): `Slot::findEffect<E>()` /
`Tile::findEffect<E>()` are the single dynamic_cast points.

---

## 4. Interaction contexts (the mutable outcomes)

Each interaction builds a small struct that flows through the in-scope modifiers,
then is applied, then recorded as a `TurnEvent`. The struct is the single place an
interaction's result can be changed.

```cpp
struct MergeContext {       // IMPLEMENTED (slices 2 + 4)
    Slot*  slot;            // where the merge resolves
    int    sourceValue;     // pre-merge value of each tile (immutable input)
    int    resultValue;     // mutable: modifiers may change the merged value
    int    coinReward = 0;  // accumulator: modifiers add; routed through CoinContext
    bool   destroyResult = false;   // FUTURE chip: "destroy the tile on merge here"
};

struct CoinContext {        // IMPLEMENTED (slice 4): every coin GAIN flows through
    Slot*  source;          // slot the coins originate over (nullable)
    int    amount;          // mutable: e.g. a chip multiplies it
};

struct SpawnContext {       // FUTURE
    Slot*  slot;
    int    value;           // mutable: modifiers may change the spawned value
};
```

This is what the event log alone could **not** give us: chips need to *alter*
`resultValue` / `destroyResult` / `amount` before they're applied. The log records
the *final* outcome afterward (`CoinsGained` carries final AND base amounts).

NOTE: the `EffectContext& ctx` member originally sketched here is deferred with
`EffectContext` itself (§5) — contexts stay pure data until a reactor needs to
*act* from inside a hook. Spending (negative amounts) deliberately bypasses the
coin pipeline: chips amplify income, not expenses (`GameRun::addCoins`).

---

## 5. `EffectContext` (shared mutation façade)

**IMPLEMENTED (cards slice, 2026-06-11)** — deferred out of slice 4 (no consumer
there: modifier hooks only mutate their context, per the re-entrancy rule), it
landed with the reactor hooks, its first real consumers. As built
(`effects/EffectContext.h`): a small CONCRETE adapter, not an abstract base —
it binds `GameRun& + Board& + const TurnLog&` for the duration of ONE reactor
pass (constructed by `GameRun::dispatchReactors`), and routes every mutation
through the existing guarded entry points: `addCoins` → the coin pipeline
(chips on the source slot scale a card's reward), `destroyTile` →
`Board::destroyTile` (protection + logging), `spawnTile` → `Board::spawnTileAt`.
Items still act on `GameRun&` (resolved: converge later).

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
**PINNED for the reactor pass (2026-06-12):** by scope (tile → slot → run;
board-owned and Boss join as those owners land), coord-ordered within a scope
— (x, then y), the slot map's natural order, chosen over row-major so the
implementation has ONE ordering source. The modifier pipelines keep their
existing site order (tile → slot at the interaction site).

---

## 7. Worked example — a merge, end to end

Setup: slot `S` has a chip *"×2 coins over this slot"*; the run holds a card
*"each merge of two 2s → +5 coins"*. Player merges two 2-tiles on `S`.
(As implemented in slice 4 — with the card's role played by a `CoinBonusChip`
until run scope exists.)

1. `Board` resolves the slide; a merge is about to happen on `S`.
2. Build `MergeContext{slot=S, sourceValue=2, resultValue=4, coinReward=0}`.
3. Run `onMergeResolving` in order:
   - tile effects: none.
   - slot/chip: the ×2-coins chip implements `onCoinsResolving`, not merge → skipped here.
   - run card: "two 2s → +5" sets `coinReward += 5`.
4. Apply: set merged tile value `= resultValue (4)`; `destroyResult` false.
5. Emit `TurnEvent::tileMerged(4, 2, S.coord, brickBroke)` (final values).
6. `GameRun::addCoins(coinReward=5, source=S)` → builds `CoinContext{source=S, amount=5}`
   → the ×2 chip's `onCoinsResolving` doubles it to `10` → apply +10 coins →
   emit `TurnEvent::coinsGained(10, 5, S.coord, sourced=true)`.

Both roles cooperated: card (reactor-ish modifier) added reward, chip (modifier)
scaled it, log recorded the truth. ORDER NOTE: the merge event precedes its coin
consequence in the log (cause before effect), so reactors replaying the log see
a causal narrative; this flips steps 5/6 of the original sketch.

---

## 8. Clone & persistence (retires the flag debt) — DONE (slice 3)

- All effects implement `clone()`. Both clone paths honor `Effect::isPersistent()`:
  `Slot`'s copy-ctor clones persistent slot effects, and `Board::copyStateFrom`
  rebuilds each tile from value then clones its persistent tile effects. The
  persistence rule lives ONCE, on the effect.
- `Tile::bricked` / `Tile::frozenThisTurn` are GONE as fields: they are
  `BrickEffect` (persistent, immobilizing) and `FrozenEffect` (transient,
  immobilizing) in `effects/TileTags.h`. The old Tile API (`setBricked`,
  `isBricked`, `isFrozenThisTurn`) survives as a thin facade over the tag store,
  so call sites and tests read as domain vocabulary.
- The per-turn **event log stays off the board** (owned by `Turn`, never cloned) —
  unchanged; it remains the reactor feed.

---

## 9. Cards (run-level reactors) & the log — IMPLEMENTED (2026-06-11)

- Cards are run-scoped `Effect`s held on `GameRun` (`cards`, via `addCard`), in
  acquisition order — the deterministic dispatch order for the run scope. They
  live outside the board/turn stack: they persist through undo and are never
  cloned ("the player persists").
- **Reactor dispatch is STREAMING at safe points (revised 2026-06-11)** —
  `GameRun::flushReactors(turn)` dispatches the turn's not-yet-seen events
  (tracked by `Turn::reactorCursor`) and is called wherever the world sits
  between atomic operations: **after an item use** (`GameRun::useItem` — so
  Economic Boom pays the instant the bomb goes off), **after the move sweep
  resolves** (`Turn::update` Movement — so brick-break reactions land with the
  move, same frame), **after board-resolution spawns**, and **at end of turn**
  (final flush + `dispatchTurnEnd` for the aggregate `onTurnEnd` pass, before
  `newTurn` so reactor board mutations carry into the next turn's clone).
  The cursor makes dispatch **in-order and exactly-once regardless of how many
  flushes run** — flush placement tunes LATENCY, never correctness.
- **The sweep stays atomic — deliberately.** Events emitted mid-sweep (merges,
  brick breaks) are dispatched at the first safe point after `Board::move`
  returns, never inside it: a reactor mutating the board mid-sweep could
  corrupt the movement queue or free the merging tile under its own feet
  (`mergedThisSweep` write-after-free). A future mechanic that must STOP or
  alter a sweep in progress is a MODIFIER at the interaction site (context
  hook), not a reactor — the two-role split covers it by design.
- **Event-major order** within a flush: every card sees event *i* before any
  card sees event *i+1* (the Balatro left-to-right rule).
- **No cascades**: after dispatching, the cursor jumps PAST anything the
  reactors appended (their `CoinsGained` etc. are logged — the turn record
  stays truthful — but never re-dispatched). Each event is copied out before
  dispatch (reactor appends may reallocate the log's storage).
- **Activation observability (2026-06-11)**: during the onEvent phase the
  dispatcher tells the context which card is acting; the card's FIRST mutation
  for that event logs one `CardTriggered(cardId)` — "a card fired" is itself a
  log event other effects can read (Card Ruler). One per (card, event): firing
  on three events = three (so "did it fire" vs "how many times" are both
  parsable); two mutations inside one reaction = one. A mutation ATTEMPT counts
  (e.g. Bob's brick lost to a full inventory is still an activation).
  onTurnEnd-phase mutations log nothing — every end-of-turn card is outside the
  activation count by construction.
- `ReactorCard` (`effects/Cards.h`) is the generic data+lambda substrate (the
  ItemRegistry pattern) the future CardRegistry feeds; stateful cards subclass
  `Effect` directly. The debug dump in `Turn::endTurn` stays as an independent
  verification channel (it now also shows reactor-appended events).
- **Board-scope dispatch (IMPLEMENTED 2026-06-12 — boss-design §9.2
  prerequisite):** `flushReactors` and `dispatchTurnEnd` dispatch
  BOARD-RESIDENT effects (tile- and slot-mounted, empty slots included) before
  the run-scoped cards, per event, in the pinned cross-scope order (§6).
  - **Owner identity is passed at dispatch** through
    `EffectContext::ownerSlot()` — never bound into the effect at mount, so
    board clones never rebind back-pointers. "Destroy a slot adjacent to ME"
    reads it; null while cards dispatch (no position). The Boss joins with its
    own owner accessor when the entity lands.
  - **Owner lifetime is defended**: the dispatch list is snapshot up front
    (`Board::collectBoardEffects`) and every owner re-validated on the LIVE
    board before each hook (`Board::findOwnerSlot`, pointer identity — tiles
    may have moved). An owner destroyed mid-flush ⇒ its effect is skipped; an
    effect MOUNTED mid-flush joins the next flush (the no-cascade family).
    Accepted corner: free-then-mount within one flush can reuse the freed
    effect's address, dispatching the newcomer early — benign (it points at a
    live effect that would legitimately see later events anyway).
  - Board reactions are **attribution-free** (no `CardTriggered`); revisit
    with boss content if "the boss's malus fired" must be a readable event.
- A card that is *also* a modifier (e.g. "all merges +1 coin") is included in the
  run scope of the merge dispatch (section 6), so it participates live.

---

## 10. Registries & content

Mirror `ItemRegistry`: content = data + lambdas keyed by id, in core; config-driven
later. Likely: `ChipRegistry`, `CardRegistry`, and a `SlotTypeRegistry`/factory
(Shop/Dark Shop/Event = recipes that attach the right slot effects). Mechanism in
core, policy in registries — no new control-flow per content item.

**`CardRegistry` DONE (2026-06-11)** — `core/CardRegistry.h`: `CardDef` = display
data (texture/name/description) + cost/weight + an `instantiate` factory building
the run-scoped effect (usually a `ReactorCard`). `GameRun::acquireCard(id)` mounts
it (one copy per card; `OwnedCard` keeps the def id beside the live effect so the
UI can show what's owned). Pricing through `getEffectiveCost(CardDef)` — the card
twin of the item hook. First content: **Two for Two** ("every time two 2 tiles
merge, gain 2 coins", reward sourced at the merge cell so slot chips scale it).
Shop offers every unowned card in a row below the items (the DEBUG shop offers
the whole catalogue forever, duplicates included); owned cards live in the play
screen's LEFT column with select/discard. `ChipRegistry`/`SlotTypeRegistry`
still future.

**Catalogue (2026-06-11):** `two_for_two` (reactor: 2+2 merge → +2 coins, sourced
at the cell); `economic_boom` (reactor: ItemUsed "bomb" → +3 coins; plain id
check FOR NOW — the bomb FAMILY needs an ItemDef tag system + tag-query hook,
deliberately deferred); `vase_of_two` (capability: spawnCountFactor 2);
`back_to_back` (capability: hourglassRewinds 3); `bob` (reactor: TileMerged
with brick-broke flag → grant a "brick" item, silently lost on full inventory;
if brick breaks ever get more sources, promote the flag to a dedicated
BrickBroken event). `EffectContext` grew `addItem` for Bob.

**End-of-turn aggregates (2026-06-11)** — the onTurnEnd channel's first real
content: `consume` (2 coins per ItemUsed), `red_light` (1 coin per `TileSlid` —
new event, one per tile whose CELL changed during the move, distance-
independent, merge movers included, emitted after the sweep from a before/after
identity snapshot in `Board::move`), `card_ruler` (3 coins per `CardTriggered`
activation — see §9's activation observability).

---

## 11. Migration plan (incremental, behavior-preserving, always builds)

1. **`Effect` base + `EffectContext`**; adapt `SlotEffect`/`ShopEffect` onto them.
   Behavior identical. (Foundations.)
2. **Merge interaction pipeline** in `Tile::mergeIntoSlot`/`Board`: gather scoped
   effects, run `onMergeResolving`, apply, emit the (now post-modifier) event.
   `ShopEffect` plugs in unchanged in behavior.
3. **Tile-tag system** — DONE (slice 3, with the capability queries of §3 and the
   shop's protection converted to them): brick/frozen → persistent/transient tile
   effects; both clone paths driven by `isPersistent()`; `copyStateFrom`
   special-case removed; `Board` asks `isImmobilized()`/`isProtected()` instead of
   type-checking effects.
4. **Coin pipeline** — DONE (slice 4, 2026-06-11; `EffectContext` deferred to the
   cards slice, see §5): `CoinContext` + `Effect::onCoinsResolving` +
   `MergeContext::coinReward`; `GameRun::addCoins(amount, source)` is the single
   entry point (gains run the pipeline tile→slot and emit `CoinsGained` with
   final+base amounts; spending bypasses it). First chips in
   `effects/CoinChips.h`: `CoinBonusChip` (+N on merge here) and
   `CoinMultiplierChip` (×N gains sourced here) — engine-level only, mounted
   programmatically until the mounting layer lands. Normal play unchanged
   (base merge reward stays 0; no chip is mounted outside tests).
5. **Chip mounting**: a chip is an `Effect` the player mounts/unmounts on a slot/board;
   first "destroy-on-merge" chip.
6. **Cards/reactor pass** — DONE (2026-06-11, with §5's `EffectContext` and the
   `onEvent`/`onTurnEnd` hooks; see §9). The debug dump was kept (independent
   verification), not retired. Suite covers: react+sourced reward, card×chip
   composition (chip scales a card's reward), no-cascade rule, onTurnEnd.
7. **Registries** for chips/cards/slot-types; config-driven data later.

Cross-cutting derisking — **DONE (2026-06-10)**:
- **Seeded run RNG**: `GameRun` takes an optional seed (nullopt = random); the
  seed actually used is readable via `getRunSeed()` and printed in debug builds,
  so any run can be replayed.
- **Invariant test harness**: `tests/invariants.cpp` → `SFML3_Tests` CMake target
  (plain asserts, no framework; drives the real core against a hidden window; run
  via `ctest --test-dir build -C Debug`). Pins: RNG determinism; merge emits
  exactly one event and the sweep is a re-run fixpoint (the one-shot move
  contract's substrate); clone copies `bricked` / drops `frozenThisTurn`; snapshot
  rewind restores the turn-start board; `goBack` restores the resumed turn's shop
  countdown; shop protection from destroy/wrench + `triggered` flag surviving the
  clone; item use consumed+logged. Extend this suite with each engine slice.
  Test seams added for it (useful beyond tests): `Board::spawnTileAt(coord, value)`
  (deterministic spawn; the random spawn delegates to it) and `Board::getAllTiles()`.

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

---

## 13. Undo semantics (RESOLVED 2026-06-10)

**Rule: the WORLD rewinds, the PLAYER persists.**

- **Rewound with the turn (world):** the board — slots, tiles, per-tile state —
  slot effects including the shop's `triggered` flag, the **shop spawn countdown**,
  and the turn event log.
- **Persist through a rewind (player):** coins, inventory, the held-item selection.

Rationale: the Hourglass rewinds the *consequences on the board*, not the player's
pockets — purchases stay bought, spent coins stay spent. This matches the original
intent of the turn stack and avoids snapshotting the whole run per turn.

Implementation: each `Turn` records `shopCountdownAtStart` (captured at
construction); `GameRun::goBack()` restores it after popping, so a rewound turn
replays with its original countdown instead of advancing the shop clock twice.
Board + event-log rewind were already in place (`Turn::endTurn`).

Accepted consequences:
- A shop consumed in the undone turn is back and re-activatable. Coins are
  conserved (re-buying costs again), so there is **no resource printer today**.
- **BALANCE WATCH:** once effects can award coins (coin pipeline, §4–§5), an
  earn → undo → replay loop becomes possible, because coin gains persist. That is
  a balance knob (Hourglass price/rarity, or a rewind cap), NOT an architecture
  change — do not bend the rule for it.
- Future effect state follows the same split: board/slot/tile-scoped effect state
  is cloned with the board (world ⇒ rewinds); run-scoped state (cards) persists
  (player ⇒ survives undo).

**Reactor visibility under undo (PINNED 2026-06-11; REVISED same day when
dispatch went streaming):**

- **Reactors observe events at SAFE POINTS as the turn progresses** (item use,
  post-sweep, post-spawn, end of turn — see §9), exactly once each, in order.
  Undo interplay: the log AND the reactor cursor rewind together (`endTurn`
  resets both for the replayable turn), so an undone-then-replayed turn
  re-fires its reactors from scratch, and a POPPED turn's unobserved events
  vanish with it. Rewards a reactor already granted before the pop persist
  (player) — the balance-watch family, accepted. Uniform rule, no per-event
  exceptions.
- **The Hourglass's own `ItemUsed` lands in the ARRIVAL turn's log** (the
  rewound turn the player resumes). Chronologically accurate — the rewind
  happened, then the replay experience — and observed exactly once, when the
  arrival turn completes. If a second Hourglass pops that turn too, the first
  event is rewound away with it (the rule above, applied consistently).
  Emission site: `GameRun::useItem` logs into `turns.top()` AFTER the item's
  effect resolved, which for the Hourglass is the post-pop top. Pinned by test.
