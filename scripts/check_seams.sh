#!/usr/bin/env bash
# Checks the one-way dependency rule between the code layers.
#
#   core     (include/fh/*.hpp, src/*.cpp)        may include nothing from another layer
#   nasdaq   (include/fh/nasdaq, src/nasdaq)      may include core only
#   sim      (include/fh/sim, src/sim)            may include core and nasdaq
#   net      (include/fh/net, src/net)            may include core only
#   live     (include/fh/live, src/live)          may include core, nasdaq, sim and net
#   bench    (include/fh/bench, src/bench)        may include core, nasdaq, sim, net and live
#
# Usage: check_seams.sh <project root>
# Prints every offending #include and exits with status 1 if there is any.
set -euo pipefail
root="${1:?usage: check_seams.sh <project root>}"
cd "$root"

declare -A allowed=(
  [core]=""
  [nasdaq]=""
  [sim]="nasdaq"
  [net]=""
  [live]="nasdaq sim net"
  [bench]="nasdaq sim net live"
)
layers=(nasdaq sim net live bench)

violations=0

# Prints the files that belong to a layer.
files_of() {
  local layer="$1"
  if [ "$layer" = core ]; then
    find include/fh src -maxdepth 1 -type f \( -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null
  else
    find "include/fh/$layer" "src/$layer" -type f \( -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null
  fi
}

check_layer() {
  local layer="$1" other
  for other in "${layers[@]}"; do
    [ "$other" = "$layer" ] && continue
    case " ${allowed[$layer]} " in *" $other "*) continue ;; esac
    while IFS= read -r file; do
      [ -z "$file" ] && continue
      if grep -nE "#include[[:space:]]*[<\"]fh/$other/" "$file" >/tmp/seam_hits.$$ 2>/dev/null; then
        while IFS= read -r hit; do
          echo "seam violation: $file:$hit  ($layer must not include $other)"
        done </tmp/seam_hits.$$
        violations=$((violations + 1))
      fi
    done < <(files_of "$layer")
  done
}

for layer in core "${layers[@]}"; do check_layer "$layer"; done
rm -f /tmp/seam_hits.$$

if [ "$violations" -ne 0 ]; then
  echo "$violations file(s) break the layer rule"
  exit 1
fi
echo "layer rule holds"
