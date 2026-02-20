## Before running the model using Run_Model, It should be smart to test the model first. Use the following code on windows Powershell. You can adjust the system prompt

# Path to your model
$ModelPath = "(YOUR MODEL)"

# Loads persistent memory to avoid any further debugging later on.
$memory = ""
if (Test-Path "memory.txt") {
    $memory = Get-Content "memory.txt" -Raw
}

# System prompt
$systemPrompt = @"
You are a LLM
"@

# Combine system prompt + memory for a single-shot session
$fullPrompt = @"
$systemPrompt

Persistent Memory:
$memory

Begin interaction.
"@

.\llama-cli.exe `
    -m $ModelPath `
    --ctx-size 4096 `
    --n-gpu-layers 35 `
    --temp 0.5 `
    --top-p 0.9 `
    --repeat-penalty 1.15 `
    --prompt "$fullPrompt"
## Configure this according to your hardware and adjust to make it optimal.