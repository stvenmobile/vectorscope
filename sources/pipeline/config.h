#pragma once

// Non-secret, project-wide tuning knobs - separate from secrets.h so this
// file is safe to commit as-is.
static const char* CHAT_MODEL = "llama3.2";
static const char* EMBED_MODEL = "nomic-embed-text";

// MVP: a single hardcoded question. Later this becomes a preset list / voice
// input; the pipeline itself doesn't care where the prompt comes from.
// Kept short on purpose - CPU-only inference makes every debug cycle cost
// real time, and a long answer here just adds latency without adding
// diagnostic value.
static const char* MVP_PROMPT = "What is gravity? Answer in 2-3 short sentences.";

static const int MAX_CONCEPTS = 18;
static const int TOP_K_EDGES_PER_NODE = 4;
