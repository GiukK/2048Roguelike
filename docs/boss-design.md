# Boss & Enemy System — Design

Status: **design pinned (2026-06-12); slices 1 (defeat check) and 2
(board-scope reactor dispatch) landed 2026-06-12, slices 3+ (Boss entity
onward) pre-implementation.** Companion to
`docs/effect-engine-design.md` (the engine this builds on); read that first.
All major decisions below are RESOLVED from the 2026-06-12 design session;
the few still open are marked `DECISION:`. See the memory note
`boss-enemy-design` for the session-level rationale trail.

---

## 1. Purpose & scope

Give the game stakes. Two new concepts on top of the effect engine:

- **Enemy** — a hostile presence on the board carrying malus effects that
  worsen the run's conditions (destroy slots, disable consumables, tax coins…).
- **Boss** — the final objective of an *ante* (see §6). Spawns at a defined
  moment, must be defeated to progress. Each boss has its OWN defeat
  mechanic — per-boss complexity is deliberately the content axis that keeps
  the game fresh.

Design pillars (FIRM):

- **No player HP/hearts.** Enemies hurt the BOARD, not a life bar.
- **Defeat = the board is full and no legal move exists, regardless of owned
  items** (no item-rescue simulation — deliberately cheap to evaluate, §8).
- **Boss HP is a real, visible parameter** — shown in the boss-fight UI
  banner. Hidden boss health is explicitly rejected ("è sempre fastidioso
  non sapere la vita del boss").
- **The board NEVER resets.** Added slots, lost slots, heavy tiles and
  strategic layouts persist across the whole run; the ante is a phase overlay
  on one continuous world.

---

## 2. The Boss entity (body/behavior split)

A boss is **not a Tile**. Tile's invariants — one cell, value-keyed sprite,
participation in the movement sweep, equal-value merging — do not stretch to
a 2x2 non-merging body with HP; forcing them would scatter `if (isBoss)`
through clean code. So:

- **The BODY is a first-class board-resident entity** (`Boss`): a footprint
  (set of coords over existing slots), HP, a `defId`, and its own render path
  (boss textures are per-def, not value-keyed). It lives on `Board`, so it
  **clones with the turn** — boss state participates in the undo stack like
  any world state (§7).
- **THE LINE TO HOLD: the Boss is a new effect OWNER/SCOPE, not a parallel
  engine.** `Effect.h` already defines scope = owner; the boss joins Tile /
  Slot / Board / GameRun as a fifth owner. Wherever a boss touches shared
  systems — merges, coins, movement, items — it goes through the SAME
  pipelines and hooks (MergeContext, CoinContext, the event log, capability
  queries). This is not purism: it is what makes chips and cards COMPOSE with
  boss fights ("+50% damage from this slot", "when the boss takes damage,
  gain 2 coins") without any boss knowing they exist.
- **Bespoke per-boss logic lives in the `BossDef`** (registry entry, §5):
  data + lambdas, or an `Effect`/`Boss` subclass for stateful mechanics
  (phase counters, invincibility windows). That is content, and arbitrarily
  weird content is fine — the cards already prove the data+lambda substrate
  carries stateful logic.

Boss maluses (the "enemy" half of a boss) are ordinary `Effect`s mounted on
the boss: a reactor on `Moved` ("each move: chance to destroy an adjacent
slot"), a capability ("consumables disabled while I'm on the board" — a new
`disablesItemUse()` aggregated like `protectsOwner()`, gated in
`GameRun::useItem`, already the single entry point).

---

## 3. The attack interaction (occupant decision point)

The ONE genuinely new interaction. Today `Board::resolveNextTileMove` knows
three answers about the next cell: slot-with-tile (merge or stop), empty slot
(slide), no slot (stop). Boss occupancy adds a fourth, resolved by asking the
occupant ONE question:

```cpp
// Asked by movement resolution when a tile would enter a boss-occupied cell.
// The closed outcome vocabulary keeps Board species-blind: it applies the
// outcome without ever knowing WHICH boss (or future occupant) answered.
struct IncomingResolution {
    enum class Kind { Block,   // tile stops before the cell (a wall)
                      Hit };   // tile is consumed, boss takes damage
    Kind kind;
    int  proposedDamage = 0;   // boss's own formula (tile value, flat 1, 0…)
};
IncomingResolution Boss::resolveIncoming(const Tile& attacker);
```

- **Default (RESOLVED): try merge, then attack.** The `BossDef` base
  implementation: if the attacker matches the boss's displayed target value →
  `Hit` with the boss's per-hit damage; otherwise → `Hit` with
  `damage = attacker value` (absorb). The "high HP, damaged by everything"
  boss IS the default; other bosses RESTRICT (only the target value, only on
  vulnerable turns → `Block` otherwise). Default-generous, content-restricts.
- **Bosses do NOT reuse the shop's phantom-tile trick.** The shop works by
  holding a real Tile on a real Slot; a 2x2 entity holds no tiles. ALL
  tile-vs-boss contact goes through `resolveIncoming` — the "target value" of
  hit-restricted bosses is boss UI state, not a phantom tile.
- **Damage flows through an `AttackContext` pipeline** (mirrors
  `MergeContext`): build context → run in-scope `onAttackResolving` modifiers
  → apply → emit the event. This is the chips/cards composition point.

```cpp
struct AttackContext {
    Boss*  boss;            // who is being hit
    Tile*  attacker;        // the incoming tile (immutable input)
    int    damage;          // mutable: modifiers may scale it
    bool   consumeAttacker = true;  // mutable: future "ghost strike" effects
};
```

  `DECISION:` pipeline scoping for slice one — recommend attacker's tile
  effects + run scope (cards) now; slot scopes join when the chip mounting
  layer lands (a chip mounted on the attacker's origin or the boss's slot
  scaling damage is the obvious future content).
- **The sweep stays atomic** (engine doc §9): the attack outcome is applied
  at the interaction site like a merge is; anything reacting to it (cards)
  fires at the next safe point via the streaming reactor dispatch.

New `TurnEvent` types (the existing generic POD fields fit — no new struct):
`BossSpawned`, `BossDamaged` (valueA = final damage, valueB = HP after,
coord = attacked cell), `BossDefeated`. Cause-before-consequence order holds:
the `BossDamaged` event follows the attack that produced it, and cards
reacting to fight progress get their synergy hook for free.

---

## 4. Boss death & scars

- **`onDefeat` hook in `BossDef`** (receives `EffectContext`) — death effects
  are content, malleable per boss (RESOLVED). The primitives all exist:
  `removeSlotUnder` to leave holes, `addItem` / `spawnTile` for loot.
  Default: footprint cells freed intact.
- **Scars are PERMANENT (RESOLVED):** slots a boss destroys during the fight
  stay lost after it dies. No restore code exists or should exist — slots are
  world state and the world persists (§1). The Mount item is the counterplay;
  this is a deliberate economy, not an oversight.

---

## 5. `BossRegistry` & content model

Mirror `CardRegistry` (engine doc §10): `BossDef` = display data (name,
textures, footprint shape) + HP + per-ante scaling + the behavior lambdas
(`resolveIncoming` override, `onDefeat`, mounted malus effects) + an
`instantiate` factory. Mechanism in core, policy in the registry — a new boss
is a registry entry, never new control flow in Board/GameRun.

First catalogue targets (from the design session, in rough build order):

1. **Brute** — high HP, default resolveIncoming (damaged by everything).
   The vertical-slice boss: exercises entity, attack, HP UI, death.
2. **Shifter** — lower HP, only its displayed target value damages it,
   re-rolls the target after every hit.
3. **Purist** — one tile type damages it, fixed.
4. **Sleeper** — invincible for 3 turns, on the 4th takes damage from all
   orthogonally adjacent tiles (needs board-scope reactor dispatch, §9.2).

---

## 6. The ante loop

Balatro-shaped, Isaac-doored. Per ante:

```
free 2048 play → shop(s) → BOSS FIGHT → reward → next ante (harder)
```

- **Ante state lives in `GameRun`** next to the shop policy knobs it governs:
  ante number, `AntePhase { FreePlay, BossFight, Reward }`, and the anti-farm
  budget. Difficulty scaling (boss HP, shop prices, malus intensity) keys off
  the ante number through the existing modular hooks (`getEffectiveCost`,
  `ShopTileValueStrategy`, …).
- **Anti-farm (FIRM): no infinite shops.** The per-ante budget — ~3 shops, or
  a global per-ante countdown — doubles as the boss-fight trigger: budget
  exhausted → the boss arrives. Time/resource-triggered, not player-chosen.
  `DECISION:` budget form + numbers (3 shops vs countdown) — balance-level,
  pick at implementation; both are counters on the ante state reading the
  existing shop machinery.
- `DECISION:` shop ACCESS mechanic may later migrate from the spawn countdown
  to score-on-merge accumulation ("earn shops by merging"). Substrate is
  ready either way (TurnLog aggregates / a run-scoped log consumer); defer.
- **During the fight (RESOLVED):** shop countdown/spawning FROZEN (one gate
  in `advanceShopState` off the ante phase); normal tile spawns CONTINUE —
  they are both the attack resource and the board-fill threat that makes
  fights dangerous without player HP.
- Minor enemies between fights: undecided ("forse no"). If they happen they
  are cheaper instances of the same substrate (entity or hostile tile-tag,
  no objective role). Nothing in this design blocks or depends on them.

---

## 7. Rewind semantics in fights (extends engine doc §13)

**Boss state is board-scoped: a rewind undoes boss damage.** The
attack→rewind→attack-again damage-stacking alternative was considered and
rejected (RESOLVED) — pinned rationale:

- **Uniformity.** HP, phase counters, re-rolled target values: ALL boss state
  rewinds together under one clone rule. Persistent-HP would force a
  per-field "rewinds or persists?" decision for every boss design — a design
  tax and a clone-bug surface on each one.
- **Balance.** Persistent HP makes hourglass count a direct damage
  multiplier, coupling every boss's HP budget to the hourglass economy.
  Board-scoped HP makes the in-fight Hourglass a RETRY (reposition, undo the
  boss's malus, land a better hit), not a DUPLICATE.
- **The fantasy survives as CONTENT.** A card "rewinds damage the boss"
  works via the pinned Hourglass semantics (engine doc §13): its `ItemUsed`
  lands in the ARRIVAL turn's log, the reactor fires post-rewind, the damage
  applies to the rewound world and sticks. Opt-in, shop-priced, synergizes
  with Back to Back, zero boss-structure change. A boss WEAK to repetition is
  the same idea as a gimmick.

**Double doors (RESOLVED):** the turn stack CUTS at fight START **and** fight
END (Isaac boss-room doors).

- Start: no rewinding out of the fight once entered.
- End: the reward is run-scoped (player ⇒ persists); rewinding past the
  killing blow would resurrect the boss with the reward already pocketed and
  desync the run-scoped ante phase from the board. Cut both sides; within the
  fight everything rewinds together — zero special cases.
- The cuts also bound the previously-unbounded turn stack (watch item:
  retired by this design). NOTE: `getTurnCount()` derives from stack size
  today — add a run-level turn counter when the cuts land.

---

## 8. The defeat check

`Board::hasLegalMove()` — a pure const predicate, no mutation: does any tile
have a slide, a merge, or a boss-attack available in some direction? Mirrors
`resolveNextTileMove`'s decision logic (immobilized tiles, holes, occupancy).

- **Attacking a boss IS a legal move** — a board packed solid against an
  attackable boss is not a loss; feeding it tiles is the escape valve.
- Checked at a safe point (end of turn, after spawns). On failure GameRun
  signals defeat through a callback (the `ShopCallback` pattern — GameRun
  never knows the StateManager) and PlayState presents the game-over screen.
- Items are deliberately ignored (FIRM): a full board with a bomb in the
  inventory is still a loss. Keeps the predicate cheap and the rule legible.

---

## 9. Engine prerequisites (close before boss content)

Two real gaps, both identified against the current code:

1. **Cell occupancy in movement resolution** (§3): the fourth answer in
   `resolveNextTileMove` + the `IncomingResolution`/`AttackContext` plumbing.
   Localized to `Board`; the merge path is untouched.
2. **Board-scope reactor dispatch.** ~~`GameRun::flushReactors` /
   `dispatchTurnEnd` iterate ONLY the run-scoped cards today.~~ Boss reactors
   (Sleeper's turn counter, the slot-destroyer malus) are board-resident.
   Extending dispatch to board-scoped owners must solve two wrinkles cards
   never had:
   - **Owner lifetime:** cards are stable during a flush; board occupants are
     not (a reactor can destroy another pending reactor's owner mid-flush).
     Collect the dispatch list defensively up front — same family as the
     existing copy-events-before-dispatch rule.
   - **Owner identity:** "destroy a slot adjacent TO ME" needs the effect to
     know where it lives. Cards never needed this; the board-scope dispatch
     must pass the owner (or bind it at mount). Decide the interface here,
     once, not per boss.

   **CLOSED (2026-06-12, slice 2):** both legs now dispatch board-resident
   effects before the cards. Owner identity = `EffectContext::ownerSlot()`,
   passed at dispatch, never bound at mount (no clone rebinding). Owner
   lifetime = snapshot (`Board::collectBoardEffects`) + per-hook live
   re-validation (`Board::findOwnerSlot`); mid-flush mounts wait for the next
   flush. Full semantics in the engine doc §9. The Boss entity (slice 3) will
   extend the same enumeration/validation pair with its own owner accessor.

Smaller follow-ons, already anticipated elsewhere: `MergeContext::
destroyResult` (planned for destroy-on-merge chips; bosses don't need it but
the attack work neighbors it), the `disablesItemUse()` capability + gate, the
three `TurnEvent` types (§3).

---

## 10. UI

- **Boss-fight banner**: boss name + HP bar/number at the top of the play
  screen while `AntePhase::BossFight` (PlayUI reads through GameRun, the
  countdown-display pattern). Target-value bosses show their current target
  on the banner or the body.
- **Boss rendering**: per-def texture over the footprint, drawn by the Boss
  entity in the board layer — Tile's value-keyed sprite path is not involved.
  (HUD pixel-positioning debt is a known separate item — the UI overhaul in
  the camera roadmap; don't couple it here.)

---

## 11. Migration plan (incremental, each slice lands green)

1. **Defeat check** (§8) — smallest slice, gives the game stakes before any
   boss exists. `Board::hasLegalMove` + game-over callback + screen.
   Invariants: predicate true/false fixtures (full board, immobilized-only,
   hole-locked).
   **DONE (2026-06-12):** `Board::hasLegalMove()` (pure const, mirrors the
   sweep's slide/merge gates; shop-feed counts, holes/immobilized/cap don't);
   `GameRun::setDefeatCallback` + latched `isDefeated()`, checked in `newTurn`
   on the freshly cloned board (post shop-lifecycle/reactors); `GameOverState`
   modal (frozen-backdrop pattern, double-pop to menu). All headless per the
   boundary rule. Fixtures + shop-valve + latch tests in invariants.cpp.
2. **Board-scope reactor dispatch** (§9.2) — the load-bearing engine
   extension; ship with a test-only board reactor before any boss uses it.
   **DONE (2026-06-12):** see §9.2's CLOSED note. The test-only board
   reactors are `ReactorCard`s mounted on slots/tiles in invariants.cpp —
   deliberately the SAME substrate as the cards (proves "same pipelines, not
   a parallel engine"). Covered: owner identity, owner destroyed mid-flush,
   mid-flush mounting deferred, board-before-cards order, attribution-free
   board reactions, the aggregate onTurnEnd leg, chips on empty slots.
3. **Minimal Boss vertical slice** — 1x1 Brute: entity + clone + occupancy +
   `resolveIncoming` default + `AttackContext` + `BossDamaged`/`BossDefeated`
   events + HP banner + death (default `onDefeat`). Spawned by debug key, no
   ante yet. Invariants: boss clones with HP, attack exactly-once per sweep,
   defeat emits + frees footprint, undo rewinds damage.
4. **Ante machine** (§6, §7) — phases, double-door stack cuts (+ run-level
   turn counter), shop budget as fight trigger, shop freeze during fights,
   reward phase. Invariants: stack depth across the doors, countdown frozen
   in-fight, reward survives further play.
5. **First real boss + fight UI polish** — Brute promoted to ante content;
   banner, spawn/death presentation.
6. **Catalogue growth** — Shifter, Purist, Sleeper (first consumer of
   board-scope reactors), 2x2 footprints, malus effects, `onDefeat` variety,
   per-ante difficulty scaling.

Extend `tests/invariants.cpp` with every slice (the suite is the project's
regression net — 267 checks as of 2026-06-12, slices 1–2 included).
