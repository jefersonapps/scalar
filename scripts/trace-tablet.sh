#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake --build "$project_dir/build" --target scalar --parallel
trace_file="$project_dir/build/tablet-input.jsonl"
printf '\nDiagnóstico da caneta ativado.\nPressione e solte o botão da borracha algumas vezes até ocorrer o salto.\nDepois feche o Scalar. O registro será salvo em:\n%s\n\n' "$trace_file"
SCALAR_TABLET_TRACE="$trace_file" "$project_dir/build/scalar"
printf '\nRegistro concluído: %s\n' "$trace_file"
