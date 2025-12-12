#!/usr/bin/env swift
//
// dump_chapters.swift
// Tiny helper to print all AVFoundation chapter metadata items.
//

import AVFoundation
import Foundation

func usage() {
    fputs("Usage: dump_chapters.swift <path-to-m4a/mp4>\n", stderr)
}

guard CommandLine.arguments.count == 2 else {
    usage()
    exit(1)
}

let path = CommandLine.arguments[1]
let url = URL(fileURLWithPath: path)

let asset = AVURLAsset(url: url)

let groups = asset.chapterMetadataGroups(bestMatchingPreferredLanguages: ["en"])

if groups.isEmpty {
    print("No chapter metadata groups found.")
    exit(0)
}

for (idx, group) in groups.enumerated() {
    let start = group.timeRange.start
    let dur = group.timeRange.duration
    let startStr = String(format: "%.3fs", start.seconds)
    let durStr = String(format: "%.3fs", dur.seconds)
    print("Chapter \(idx): start=\(startStr) duration=\(durStr)")
    for item in group.items {
        let identifier = item.identifier?.rawValue ?? "<id?>"
        let locale = item.locale?.identifier ?? "n/a"
        let val = item.stringValue ?? (item.value as? String) ?? "nil"
        let dataType = item.dataType ?? "n/a"
        print("  id=\(identifier) locale=\(locale) value=\(val) dataType=\(dataType)")
        if let extras = item.extraAttributes, !extras.isEmpty {
            print("    extras=\(extras)")
        }
    }
}
