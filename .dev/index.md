# Engineering records

This directory holds public project records rather than the developer-facing
site. [Developer documentation](../doc/docs/index.md) is built with MkDocs.
The root `AGENTS.md` stays in place so workspace agents can discover its
instructions.

| Record | Purpose |
| --- | --- |
| [Plan](plan.md) | Long-term platform scope and gates |
| [Development agent](development-agent.md) | Staged on-device service plan and acceptance gates |
| [Status](status.md) | Measured results and current evidence boundaries |
| [Research](research.md) | Initial technical survey |
| [Research log](research-log.md) | Experiments and unanswered questions |
| [Concurrency plan](thread-a11.md) | A11 thread migration notes |
| [Distribution plan](distribution.md) | Planned packaging and release model |
| [Recovery preservation](recovery.md) | Human-governed reference-device preservation notes |

Focused ABI, source and vendor experiments are kept beside these records or
under [`research/`](research/). Build artifacts, firmware, private device data
and emulator runtime state remain outside version control.
