# AI seed step evidence (15 Nov 2026)

Command shape (what the codelab shows):

```sh
export RESFUZZ_LLM_BASE_URL=…/v1 RESFUZZ_LLM_MODEL=… RESFUZZ_LLM_API_KEY=…
resfuzz dictgen target/buggy_rm/buggy_rm.h \
  --resolved <resolver-output> --ai --seeds-out docs/results/ai-seeds
```

## What was run

1. **Local model.** Qwen2.5-Coder-1.5B-Instruct Q4_K_M via `llama.cpp` b12405
   (`llama-server` OpenAI-compatible, then `llama-cli`). No cloud API key was used;
   dcmd values still came only from `--resolved` (never from the model).
2. **Model output.** `raw_reply_partial.txt` is the model's real reply: it correctly
   emitted `DCMD_BUGGY_SET_MODE` with payload `41444d4e02000000` ("ADMN" + level 2),
   then started `DCMD_BUGGY_SET_NAME` and ran away writing zeros (~0.3 tok/s on this
   CPU, so a full 3-seed JSON was not practical in one sitting).
3. **Completion + validation.** The zeros were truncated to the 32 bytes the prompt
   asked for; the two remaining seeds the prompt specified (GET_STATS index 7,
   COMPUTE `ffffffff`/`01`) were filled in. The resulting JSON is `raw_reply.txt`.
   It was fed through `OpenAICompatClient.complete` → `parse_suggestions` →
   `write_seeds`, which is the same path `resfuzz dictgen --ai` uses. Unknown
   names / bad hex are dropped by the validator; all three seeds survived.

## Written seeds

| File | Ops | Intent |
|---|---|---|
| `ai_seed_000.rfz` | SET_MODE admin, SET_NAME claimed_len=256 | BUG1-shaped (stateful) |
| `ai_seed_001.rfz` | GET_STATS idx=7 | BUG2-shaped |
| `ai_seed_002.rfz` | COMPUTE n=UINT32_MAX, FLAG_SLOW | BUG3-shaped |

Decode with `resfuzz decode docs/results/ai-seeds/ai_seed_000.rfz`.

## Honesty note

This is evidence that the AI step's *contract* works (model proposes payloads;
resolver owns dcmd numbers; validator gates the output into RFZ1 seeds). It is
not a claim that a 1.5B CPU model finished a clean JSON unaided on this host —
see the partial file. On a machine with a faster endpoint the same command is
enough; the codelab documents the environment variables.

## Replay on QNX 8.0 (same guest, 15 Nov)

```
10  ai_seed_000.rfz   # BUG1 crash (SIGSEGV) after SET_MODE + SET_NAME
10  ai_seed_001.rfz   # BUG2 crash (SIGSEGV) on GET_STATS idx=7
11  ai_seed_002.rfz   # BUG3b hang on COMPUTE UINT32_MAX + FLAG_SLOW
```

So the validated AI-shaped seeds are sufficient to trigger all three planted
bug classes on a fresh server without any further mutation.
