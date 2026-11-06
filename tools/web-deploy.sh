#!/usr/bin/env bash
# Deploys the browser simulator (Devices/CardenzaWeb, built into build/web/site) to Vercel as a
# static site, production at https://cardenza-synth.203.io.
#   tools/web-deploy.sh                 build with `make web`, then deploy to production
#   SKIP_BUILD=1 tools/web-deploy.sh    deploy the existing build
#   PREVIEW=1 tools/web-deploy.sh       a preview deployment instead of production
# One-time setup: `npx vercel login` (sign in in the browser), then the first run links the
# Vercel project (VERCEL_PROJECT, default cardenza-synth, in VERCEL_SCOPE if set). The domain is
# added to the project with `npx vercel domains add cardenza-synth.203.io cardenza-synth` and a
# DNS record `CNAME cardenza-synth -> cname.vercel-dns.com` at the 203.io DNS provider.
set -euo pipefail

cd "$(dirname "$0")/.."
PROJECT=${VERCEL_PROJECT:-cardenza-synth}
WEB_BUILD=${WEB_BUILD:-build/web}
SITE=$WEB_BUILD/site
# Staged outside the repository: from inside it the CLI attaches the last commit, and Vercel blocks
# a deployment whose commit author is not on the Vercel account.
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/cardenza-web-deploy.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT
LINK=$WEB_BUILD/.vercel
VERCEL=(npx --yes vercel@latest)
SCOPE_ARGS=()
[[ -n ${VERCEL_SCOPE:-} ]] && SCOPE_ARGS=(--scope "$VERCEL_SCOPE")

[[ ${SKIP_BUILD:-0} == 1 ]] || { make web && make learn; }
for file in index.html cardenza.js cardenza.wasm cardenza-web.js cardenza-web.css cardenza-audio-worklet.js; do
  [[ -f $SITE/$file ]] || { echo "web-deploy: $SITE/$file missing, run make web" >&2; exit 1; }
done

# Stage only the page files; the Vercel link survives between runs next to the build.
cp "$SITE"/index.html "$SITE"/*.js "$SITE"/*.css "$SITE"/*.wasm "$STAGE"/
# The Learn site (/learn: the guide and the manual, wiki/), when built (make learn).
[[ -d $SITE/learn ]] && cp -r "$SITE"/learn "$STAGE"/learn
cp Devices/CardenzaWeb/vercel.json "$STAGE"/
[[ -d $LINK ]] && cp -r "$LINK" "$STAGE"/.vercel

if [[ ! -d $STAGE/.vercel ]]; then
  "${VERCEL[@]}" link --yes --project "$PROJECT" --cwd "$STAGE" "${SCOPE_ARGS[@]}"
fi

if [[ ${PREVIEW:-0} == 1 ]]; then
  "${VERCEL[@]}" deploy --yes --cwd "$STAGE" "${SCOPE_ARGS[@]}"
else
  "${VERCEL[@]}" deploy --prod --yes --cwd "$STAGE" "${SCOPE_ARGS[@]}"
fi
rm -rf "$LINK"
cp -r "$STAGE"/.vercel "$LINK"
