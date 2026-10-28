# Keyboard input and interactive demo

Milestone dated October 25, 2026. Window arrow keys reach an ARM64 C client through local **emu:pad**; the guest computes movement/pixels. This demo does not implement native Switch 2 HID/GPU interfaces.

```powershell
.\tools\run_interactive.ps1
# Equivalent after building Release:
.\build\Release\Instaurix.exe .\tests\programs\hle_interactive\program.elf 50000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --window
```

The launcher builds Release and opens the supplied resources. **Arrow keys** move the ship; **Escape** requests clean guest exit. Closing the window interrupts execution. The demo also exits after **60 frames**, leaving the last image visible until close. Real pacing includes computation and 100 ms waits, without guarantees.

## Window input

Only messages addressed to this window are processed: no global keyboard hooks/polling. Key release updates held bits; auto-repeat does not toggle them. Focus loss, close and reopen clear held keys. Escape additionally latches a quit request so a brief press during rendering is retained, separately from held state; close/reopen resets it.

Before each run slice, Machine::setButtons receives the state. Input is processed during waits too. Brief directional changes between guest reads may be missed: this first interface provides state, not an event queue.

## Local service

SM accepts the eight-byte name `emu:pad\0`. CMIF **0 ReadButtons**, without parameters/descriptors, returns a u64 mask:

| Bit/value | Meaning |
| --- | --- |
| 0 / 1 | Left |
| 1 / 2 | Right |
| 2 / 4 | Up |
| 3 / 8 | Down |
| 4 / 16 | Quit |

Initial/new-process state is zero. Host injection rejects unknown bits without replacing state, requires HLE, and fails after exit/fault. Without a window/injection all keys remain released. Input itself needs no file mount. Common IPC/session rules apply; reading neither advances time nor accesses PC keyboard APIs.

## Guest behavior and validation

The C client reuses sprite.txt, emu:fs and emu:disp. Initial position x=48,y=16; input is read before every frame. A single direction moves four pixels; opposite directions cancel per axis; diagonals work. The 64×64 sprite stays in a 160×96 image: x∈[0,96], y∈[0,32].

Seven u64 report fields at 0x12000: status, loaded, presented, hash, x, y, max_frames. Success has status=1 and loaded=272. max_frames defaults to 60, is bounded to 600, and is lowered for automation.

Tests cover press/release/repeat/focus loss, hidden-window messages on the test's own Windows thread, brief Escape and closure. They are not manual visual play sessions. Deterministic guest scenarios include diagonals/opposites, all pixels of eight frames, budget equivalence, all edges, Escape before any frame, IPC errors and reset, without using the user's real keyboard.

Next was comparing these foundations against a broader application and identifying blocking instructions/services. This remains a project reference demo rather than Switch 2 game compatibility.
