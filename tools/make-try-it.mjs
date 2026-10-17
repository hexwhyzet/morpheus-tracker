#!/usr/bin/env node
// Builds docs/try-it.svg — the "scan this, then point at the painting" card
// in the README. Regenerate it whenever the demo URL changes:
//
//   node tools/make-try-it.mjs https://your-demo-url
//
// The painting is inlined as a data URI so the card is a single file. It is
// downscaled on the way in; 600px is plenty for tracking off a screen.

import {execFileSync} from 'node:child_process'
import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs'
import {tmpdir} from 'node:os'
import {join} from 'node:path'

const url = process.argv[2]
if (!url) {
    console.error('usage: node tools/make-try-it.mjs <url>')
    process.exit(1)
}

const root = new URL('..', import.meta.url).pathname
const tmp = mkdtempSync(join(tmpdir(), 'tryit-'))

// QR, as paths we can drop straight into the card
const qr = execFileSync(
    'npx',
    ['--yes', 'qrcode', '--type', 'svg', '--error-correction', 'M', '--output', '/dev/stdout', url],
    {encoding: 'utf8'},
)
const qrSize = /viewBox="0 0 (\d+)/.exec(qr)[1]
const qrPaths = qr.match(/<path[^>]*\/>/g).join('')

// Painting, downscaled and inlined
const small = join(tmp, 'target.jpg')
execFileSync('sips', [
    '-Z', '600', '-s', 'format', 'jpeg', '-s', 'formatOptions', '72',
    join(root, 'demo/public/default-target.jpg'), '--out', small,
], {stdio: 'ignore'})
const painting = readFileSync(small).toString('base64')

// The painting carries the card: it is what the camera has to see, so it
// gets the height. QR and copy stack in a narrow column beside it.
const W = 900
const QR = 230
const dims = execFileSync('sips', ['-g', 'pixelWidth', '-g', 'pixelHeight', small], {encoding: 'utf8'})
const srcW = Number(/pixelWidth: (\d+)/.exec(dims)[1])
const srcH = Number(/pixelHeight: (\d+)/.exec(dims)[1])

const IMG_H = 680, IMG_W = Math.round(IMG_H * srcW / srcH)
const IMG_X = W - 56 - IMG_W
const H = 124 + IMG_H + 44
const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${W}" height="${H}" font-family="-apple-system, BlinkMacSystemFont, 'Segoe UI', Inter, Helvetica, Arial, sans-serif">
  <style>
    .plate { fill: #ffffff; stroke: #e2e4ea; }
    .head  { fill: #16161a; font-weight: 600; }
    .body  { fill: #5b6070; }
    .step  { fill: #8d93a1; font-weight: 600; letter-spacing: 1.2px; }
    @media (prefers-color-scheme: dark) {
      .plate { fill: #1e1f25; stroke: #343742; }
      .head  { fill: #f2f3f5; }
      .body  { fill: #9197a8; }
      .step  { fill: #6f7484; }
    }
  </style>

  <rect class="plate" x="0.5" y="0.5" width="${W - 1}" height="${H - 1}" rx="14"/>

  <text class="head" x="44" y="62" font-size="24">Try it on your phone</text>

  <text class="step" x="44" y="108" font-size="11">1 — SCAN</text>
  <rect x="44" y="124" width="${QR}" height="${QR}" rx="8" fill="#ffffff"/>
  <g transform="translate(44 124) scale(${QR / qrSize})">${qrPaths}</g>

  <text class="body" x="44" y="${124 + QR + 44}" font-size="14">Hold the phone close</text>
  <text class="body" x="44" y="${124 + QR + 66}" font-size="14">enough that the painting</text>
  <text class="body" x="44" y="${124 + QR + 88}" font-size="14">fills the frame — or</text>
  <text class="body" x="44" y="${124 + QR + 110}" font-size="14">print it.</text>

  <text class="body" x="44" y="${124 + QR + 152}" font-size="14">A green quad locks on.</text>

  <text class="step" x="${IMG_X}" y="108" font-size="11">2 — POINT AT THIS</text>
  <image x="${IMG_X}" y="124" width="${IMG_W}" height="${IMG_H}"
         href="data:image/jpeg;base64,${painting}"/>
</svg>
`

writeFileSync(join(root, 'docs/try-it.svg'), svg)
console.log(`docs/try-it.svg → ${url}`)

// The README embeds the per-theme copies, not this one.
execFileSync('node', [join(root, 'tools/build-svg-themes.mjs')], {stdio: 'inherit'})
