# Animation and continuous presentation

Milestone dated October 25, 2026. An ARM64 C client presents eight frames with sleeps between them. A Windows viewer displays every frame or exports timestamped PNGs. This remains local emu:disp; native GPU/Vulkan are absent.

## Frame boundary

Machine::run(budget, true) returns **Stop::Frame** after a successful presentation. SVC processing, instruction count, development timer and quantum update occur before stopping; resumption starts at the next instruction. Ordinary run(budget) retains prior behavior. Readers capture each frame before replacement; instruction totals/presentation points are independent of slicing. The reader explicitly advances virtual time to deadlines while threads wait.

## Windows window

```powershell
.\build\Release\Instaurix.exe .\tests\programs\hle_animation\program.elf 10000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --window
```

The native viewer preserves aspect ratio, scales pixels with black borders, and converts RGB captures to GDI DIB without external graphics libraries. It handles messages between 4,096-instruction slices and during waits; window closure interrupts cleanly.

Virtual sleeps also wait in wall time while processing messages. Computation/drawing add time, so this is not calibrated CPU timing or guaranteed frame pacing. After normal exit the last image stays until close; reaching the instruction limit ends the session.

The window is Windows-only; core/sequence export are independent. Automated lifecycle testing creates a hidden window, presents two frames, handles messages and closes/reopens it. This is not visual inspection of a visible window.

## Headless export

```powershell
.\build\Release\Instaurix.exe .\tests\programs\hle_animation\program.elf 10000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --frames-out .\build\animation --frame-out .\build\animation-last.png
```

--frames-out writes frame-000001.png onward and **frames.csv**: number, virtual ns, cumulative instructions, dimensions and RGB FNV-1a hash. The log defines this run's sequence; stale unlisted files are excluded. Missing directories are created. Export failures stop rather than claim a complete capture.

Headless delays advance without wall sleeps. --frame-out still exports only the last capture. All options require HLE; the total budget remains strict across frame stops.

## Demo and tests

The guest loads sprite.txt, moves a ship horizontally and changes stars. It presents **8 frames at 0,100,…,700 virtual ms**, exiting after **4,882,772 instructions**. Report: status=1, loaded=272, presented=8 and final-frame hash.

Tests verify every pixel, ordering, time and instruction boundary, with large budgets and 997-instruction slices. Exported PNGs were independently decoded and compared with frames.csv hashes. Legacy tests remain active.

A GIF preview derived from PNGs is convenient but indexed colors/looping are not authoritative RGB captures; this milestone's preview was build/animation-preview.gif. [Interactive input](HLE_INPUT.md) followed; broader application targets and native controller/GPU interfaces remain separate work.
