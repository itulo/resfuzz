# Demo seed corpus

Three validated sequences from `docs/results/ai-seeds/` (local Qwen2.5-Coder-1.5B,
dcmd numbers from the on-target resolver). Used by `make demo` so BUG1 (stateful
SET_MODE → SET_NAME) is found quickly and reliably without waiting for the mutator
to discover admin mode by chance.
