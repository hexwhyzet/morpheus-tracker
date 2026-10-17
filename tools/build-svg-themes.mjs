#!/usr/bin/env node
// GitHub strips <style> out of inline SVG, so a class-based stylesheet gets
// thrown away and the drawing renders uncoloured. This inlines the rules as
// presentation attributes instead, once per theme:
//
//   docs/architecture.svg → docs/architecture-{light,dark}.svg
//
// The source files keep their stylesheet, so they stay editable and still
// follow the reader's theme when opened directly. Run this after touching
// one of them.

import {readFileSync, writeFileSync} from 'node:fs'

const SOURCES = ['docs/architecture.svg', 'docs/try-it.svg']
const root = new URL('..', import.meta.url).pathname

/** Pull `.cls { prop: value }` rules out of a stylesheet, in order. */
function parseRules(css) {
    const rules = []
    for (const m of css.matchAll(/([^{}]+)\{([^{}]+)\}/g)) {
        const decls = Object.fromEntries(
            m[2].split(';')
                .map((d) => d.split(':').map((x) => x.trim()))
                .filter(([k, v]) => k && v)
                .map(([k, v]) => [k, v]),
        )
        for (const sel of m[1].split(',').map((s) => s.trim())) {
            // `.slotpart rect` — a descendant selector; record the tag it targets
            const descendant = /^\.([\w-]+)\s+(\w+)$/.exec(sel)
            if (descendant) {
                rules.push({cls: descendant[1], tag: descendant[2], decls})
                continue
            }
            const plain = /^\.([\w-]+)$/.exec(sel)
            if (plain) rules.push({cls: plain[1], decls})
        }
    }
    return rules
}

function build(svg, {dark}) {
    const styleBlock = /<style>([\s\S]*?)<\/style>/.exec(svg)
    if (!styleBlock) return svg

    const css = styleBlock[1]
    const darkBlock = /@media \(prefers-color-scheme: dark\)\s*\{([\s\S]*)\}\s*$/.exec(css)
    const base = parseRules(darkBlock ? css.slice(0, darkBlock.index) : css)
    const overrides = darkBlock ? parseRules(darkBlock[1]) : []

    // Later rules win, exactly as the cascade would resolve them.
    const active = dark ? [...base, ...overrides] : base

    const resolve = (classes, tag) => {
        const out = {}
        for (const r of active) {
            if (!classes.includes(r.cls)) continue
            if (r.tag && r.tag !== tag) continue
            Object.assign(out, r.decls)
        }
        return out
    }

    // Inherited properties have to come along for the ride: a <g class="x">
    // wrapper no longer cascades once the stylesheet is gone.
    const INHERITED = new Set(['fill', 'stroke', 'font-family', 'font-weight', 'letter-spacing'])

    let out = svg.replace(/<style>[\s\S]*?<\/style>\s*/, '')

    // Walk elements, tracking the class stack so descendant rules still apply.
    const stack = []
    out = out.replace(/<(\/?)(\w+)([^>]*?)(\/?)>/g, (tag, closing, name, attrs, selfClose) => {
        if (closing) {
            stack.pop()
            return tag
        }

        const clsMatch = /class="([^"]*)"/.exec(attrs)
        const own = clsMatch ? clsMatch[1].split(/\s+/) : []
        const inherited = stack.flat()
        const decls = resolve([...inherited, ...own], name)

        if (!selfClose) stack.push(own)

        if (!Object.keys(decls).length) {
            return clsMatch ? tag.replace(/\s*class="[^"]*"/, '') : tag
        }

        const props = Object.entries(decls)
            // An element that already states a property wins over the stylesheet.
            .filter(([k]) => !new RegExp(`\\b${k}="`).test(attrs))
            .filter(([k]) => name !== 'g' || INHERITED.has(k))
            // Values can carry quotes and line breaks (font-family stacks);
            // flatten them so they survive as attribute values.
            .map(([k, v]) => `${k}="${v.replace(/\s+/g, ' ').replace(/"/g, "'")}"`)
            .join(' ')

        const cleaned = attrs.replace(/\s*class="[^"]*"/, '')
        return `<${name}${cleaned}${props ? ' ' + props : ''}${selfClose ? '/' : ''}>`
    })

    return out
}

for (const src of SOURCES) {
    const svg = readFileSync(root + src, 'utf8')
    for (const theme of ['light', 'dark']) {
        const target = src.replace(/\.svg$/, `-${theme}.svg`)
        writeFileSync(root + target, build(svg, {dark: theme === 'dark'}))
        console.log(target)
    }
}
