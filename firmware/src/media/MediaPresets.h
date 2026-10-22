#pragma once

/**
 * @file MediaPresets.h
 * @brief Built-in media variant presets (JSON specs for SynthFat) and lookup by id.
 */

#include <Arduino.h>

/**
 * @brief Media variant preset (0.2.31).
 *
 * Each preset changes exactly one dimension against its reference so HU behaviour can be
 * attributed (docs/MEDIA_VARIATION.md).
 * `demo_v1` = legacy flash image (FAT12, 3 × identical 158208 B tracks).
 */
struct MediaPreset {
    const char* id;     ///< preset id, also used in the JSON "id" field
    const char* label;  ///< human-readable description (German) for the UI
    const char* json;   ///< SynthFat media spec; nullptr = demo image
};

// clang-format off
static const MediaPreset kMediaPresets[] = {
    {"demo_v1", "Demo-Image FAT12, 3 identische Tracks (Referenz bis 0.2.30)", nullptr},
    {"cache_a", "3 Tracks 26 s, Inhalt A (seed 1/2/3)",
     R"({"id":"cache_a","spc":1,"files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"cache_b", "wie cache_a, nur Inhalt geaendert (seed 11/12/13)",
     R"({"id":"cache_b","spc":1,"files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":11},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":12},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":13}]})"},
    {"cache_place", "wie cache_a, nur Platzierung (+100 Cluster Luecke)",
     R"({"id":"cache_place","spc":1,"files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1,"gap_clusters":100},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"ident_b", "wie cache_a, nur Identitaet (Serial/Label/Volume-ID)",
     R"({"id":"ident_b","spc":1,"usb_serial":"HP0B01","fs_label":"HUPROBE_B","volume_id":"0x48500B01","files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    // Cache-Key matrix (0.3.18): pinned baseline + one identity dimension each.
    // Default SynthFat identity is label HUPROBE / vol 0x48500131 (see SynthFat.cpp).
    {"cache_a_pin", "wie cache_a, Serial fest HP0A01 (Baseline Cache-Key)",
     R"({"id":"cache_a_pin","spc":1,"usb_serial":"HP0A01","fs_label":"HUPROBE","volume_id":"0x48500131","files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"ident_serial", "wie cache_a_pin, nur Serial HP0S01",
     R"({"id":"ident_serial","spc":1,"usb_serial":"HP0S01","fs_label":"HUPROBE","volume_id":"0x48500131","files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"ident_label", "wie cache_a_pin, nur Label HUPROBE_L",
     R"({"id":"ident_label","spc":1,"usb_serial":"HP0A01","fs_label":"HUPROBE_L","volume_id":"0x48500131","files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"ident_vol", "wie cache_a_pin, nur Volume-ID 0x48500701",
     R"({"id":"ident_vol","spc":1,"usb_serial":"HP0A01","fs_label":"HUPROBE","volume_id":"0x48500701","files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"dur_mix", "Dauer 10/26/60/180 s @48k (Prefetch-Regel)",
     R"({"id":"dur_mix","spc":8,"files":[
        {"path":"D010.MP3","dur_s":10,"seed":1},
        {"path":"D026.MP3","dur_s":26,"seed":2},
        {"path":"D060.MP3","dur_s":60,"seed":3},
        {"path":"D180.MP3","dur_s":180,"seed":4}]})"},
    {"bitrate_mix", "60 s @32/128/320 kbit/s (zeit- vs. bytebasiert)",
     R"({"id":"bitrate_mix","spc":8,"files":[
        {"path":"B032.MP3","dur_s":60,"kbps":32,"seed":1},
        {"path":"B128.MP3","dur_s":60,"kbps":128,"seed":2},
        {"path":"B320.MP3","dur_s":60,"kbps":320,"seed":3}]})"},
    {"size_1_4_8", "1/4/8 MiB @128k (Streaming vs. Read-All)",
     R"({"id":"size_1_4_8","spc":8,"files":[
        {"path":"S1M.MP3","size_kib":1024,"kbps":128,"seed":1},
        {"path":"S4M.MP3","size_kib":4096,"kbps":128,"seed":2},
        {"path":"S8M.MP3","size_kib":8192,"kbps":128,"seed":3}]})"},
    {"pidrive_l3", "PiDrive L3: FAT16 16 MiB, FAVORITE/ 8M+512k+512k, MENUE/",
     R"({"id":"pidrive_l3","volume_mib":16,"spc":8,"stamp":false,"files":[
        {"path":"FAVORITE/01ROCK.MP3","size_kib":8192},
        {"path":"FAVORITE/02ANTENN.MP3","size_kib":512},
        {"path":"FAVORITE/03SWR3.MP3","size_kib":512},
        {"path":"MENUE/ABOUT.MP3","size_kib":512}]})"},
    {"folders_30", "3 Ordner x 10 Tracks 20 s (Traversierung)",
     R"({"id":"folders_30","spc":8,"files":[
        {"path":"ALBUM1/T01.MP3","dur_s":20,"seed":1},{"path":"ALBUM1/T02.MP3","dur_s":20,"seed":2},
        {"path":"ALBUM1/T03.MP3","dur_s":20,"seed":3},{"path":"ALBUM1/T04.MP3","dur_s":20,"seed":4},
        {"path":"ALBUM1/T05.MP3","dur_s":20,"seed":5},{"path":"ALBUM1/T06.MP3","dur_s":20,"seed":6},
        {"path":"ALBUM1/T07.MP3","dur_s":20,"seed":7},{"path":"ALBUM1/T08.MP3","dur_s":20,"seed":8},
        {"path":"ALBUM1/T09.MP3","dur_s":20,"seed":9},{"path":"ALBUM1/T10.MP3","dur_s":20,"seed":10},
        {"path":"ALBUM2/T01.MP3","dur_s":20,"seed":11},{"path":"ALBUM2/T02.MP3","dur_s":20,"seed":12},
        {"path":"ALBUM2/T03.MP3","dur_s":20,"seed":13},{"path":"ALBUM2/T04.MP3","dur_s":20,"seed":14},
        {"path":"ALBUM2/T05.MP3","dur_s":20,"seed":15},{"path":"ALBUM2/T06.MP3","dur_s":20,"seed":16},
        {"path":"ALBUM2/T07.MP3","dur_s":20,"seed":17},{"path":"ALBUM2/T08.MP3","dur_s":20,"seed":18},
        {"path":"ALBUM2/T09.MP3","dur_s":20,"seed":19},{"path":"ALBUM2/T10.MP3","dur_s":20,"seed":20},
        {"path":"ALBUM3/SUB/T01.MP3","dur_s":20,"seed":21},{"path":"ALBUM3/SUB/T02.MP3","dur_s":20,"seed":22},
        {"path":"ALBUM3/SUB/T03.MP3","dur_s":20,"seed":23},{"path":"ALBUM3/SUB/T04.MP3","dur_s":20,"seed":24},
        {"path":"ALBUM3/SUB/T05.MP3","dur_s":20,"seed":25},{"path":"ALBUM3/T06.MP3","dur_s":20,"seed":26},
        {"path":"ALBUM3/T07.MP3","dur_s":20,"seed":27},{"path":"ALBUM3/T08.MP3","dur_s":20,"seed":28},
        {"path":"ALBUM3/T09.MP3","dur_s":20,"seed":29},{"path":"ALBUM3/T10.MP3","dur_s":20,"seed":30}]})"},
    {"tail_probe", "3 Tracks, 40 reservierte Sektoren (LBA 24 im Reserved-Bereich)",
     R"({"id":"tail_probe","spc":1,"reserved":40,"files":[
        {"path":"TRACK1.MP3","dur_s":26.5,"seed":1},
        {"path":"TRACK2.MP3","dur_s":26.5,"seed":2},
        {"path":"TRACK3.MP3","dur_s":26.5,"seed":3}]})"},
    {"hdr_variants", "Header-Varianten: plain/ID3v1/APIC 256k/Xing/VBR",
     R"({"id":"hdr_variants","spc":1,"files":[
        {"path":"H_PLAIN.MP3","dur_s":26.5,"seed":1},
        {"path":"H_ID3V1.MP3","dur_s":26.5,"seed":2,"id3v1":true},
        {"path":"H_APIC.MP3","dur_s":26.5,"seed":3,"apic_kib":256},
        {"path":"H_XING.MP3","dur_s":26.5,"seed":4,"xing":true},
        {"path":"H_VBR.MP3","dur_s":26.5,"seed":5,"vbr":true,"kbps":128}]})"},
    {"frag_mix", "3 Tracks 60 s @128k, 64-KiB-fragmentiert verschraenkt",
     R"({"id":"frag_mix","spc":1,"fragment_kib":64,"files":[
        {"path":"F1.MP3","dur_s":60,"kbps":128,"seed":1,"frag":true},
        {"path":"F2.MP3","dur_s":60,"kbps":128,"seed":2,"frag":true},
        {"path":"F3.MP3","dur_s":60,"kbps":128,"seed":3,"frag":true}]})"},
    {"id3_cover", "ID3-Texte + echte JPEG-Cover 320x320 (live aenderbar), ID3v1 auf T2",
     R"({"id":"id3_cover","spc":1,"files":[
        {"path":"COVER1.MP3","dur_s":26.5,"seed":1,"title":"Cover Eins","artist":"HU-Probe A","album":"Album Rot","cover":1},
        {"path":"COVER2.MP3","dur_s":26.5,"seed":2,"title":"Cover Zwei","artist":"HU-Probe A","album":"Album Gruen","cover":2,"id3v1":true},
        {"path":"COVER3.MP3","dur_s":26.5,"seed":3,"title":"Cover Drei","artist":"HU-Probe B","album":"Album Blau","cover":3},
        {"path":"NOCOVER.MP3","dur_s":26.5,"seed":4,"title":"Ohne Cover","artist":"HU-Probe B","album":"Album Blau","cover_slot":true}]})"},
    // Long drives: no two files alike (seed, length, bitrate, header, tags, depth), so a
    // repeating read pattern cannot come from identical files (Feldtest 2026-10-20).
    {"drive_mix", "Heimfahrt-Mix: 40 Dateien, 12 Ordner (4 Ebenen), 96-320k, VBR, Tags, Cover",
     R"({"id":"drive_mix","spc":32,"fragment_kib":256,"files":[
        {"path":"INTRO.MP3","dur_s":15,"kbps":128,"seed":1,"title":"Intro","artist":"HU-Probe","album":"Heimfahrt","track":1,"cover":1},
        {"path":"NOTAG.MP3","dur_s":45,"kbps":128,"seed":2,"no_id3v2":true},
        {"path":"ROCK/R01.MP3","dur_s":150,"kbps":192,"seed":3,"title":"Gitarrenwand","artist":"Die Verstaerker","album":"Lautstaerke","track":1,"cover":2},
        {"path":"ROCK/R02.MP3","dur_s":180,"kbps":256,"seed":4,"title":"Rueckkopplung","artist":"Die Verstaerker","album":"Lautstaerke","track":2,"cover":2,"xing":true},
        {"path":"ROCK/R03.MP3","dur_s":200,"kbps":192,"seed":5,"title":"Feedback Loop","artist":"Die Verstaerker","album":"Lautstaerke","track":3,"cover":2,"id3v1":true},
        {"path":"ROCK/R04.MP3","dur_s":170,"kbps":320,"seed":6,"title":"Endstufe","artist":"Die Verstaerker","album":"Lautstaerke","track":4,"cover":2,"frag":true},
        {"path":"ROCK/CLASSIC/C01.MP3","dur_s":210,"kbps":160,"seed":7,"title":"Alte Schule","artist":"Classic Five","album":"Archiv 1971","track":1,"cover":3},
        {"path":"ROCK/CLASSIC/C02.MP3","dur_s":190,"kbps":160,"seed":8,"title":"Roehrenklang","artist":"Classic Five","album":"Archiv 1971","track":2,"cover":3},
        {"path":"ROCK/CLASSIC/C03.MP3","dur_s":240,"kbps":128,"seed":9,"title":"Bandmaschine","artist":"Classic Five","album":"Archiv 1971","track":3,"cover":3,"id3v1":true},
        {"path":"ROCK/CLASSIC/C04.MP3","dur_s":160,"kbps":224,"seed":10,"title":"Vinyl Knistern","artist":"Classic Five","album":"Archiv 1971","track":4,"cover_slot":true},
        {"path":"ROCK/CLASSIC/LIVE/L01.MP3","dur_s":300,"kbps":192,"seed":11,"title":"Live Eroeffnung","artist":"Classic Five","album":"Live in Halle 3","track":1,"cover":4,"xing":true},
        {"path":"ROCK/CLASSIC/LIVE/L02.MP3","dur_s":260,"kbps":192,"seed":12,"title":"Live Zugabe","artist":"Classic Five","album":"Live in Halle 3","track":2,"cover":4},
        {"path":"ROCK/CLASSIC/LIVE/L03.MP3","dur_s":280,"kbps":128,"seed":13,"title":"Live Abschied","artist":"Classic Five","album":"Live in Halle 3","track":3,"cover":4,"vbr":true},
        {"path":"ROCK/CLASSIC/LIVE/1977/B01.MP3","dur_s":330,"kbps":256,"seed":14,"title":"Bootleg Seite A","artist":"Unbekannt","album":"Bootleg 1977","track":1,"apic_kib":512,"cover":1},
        {"path":"ROCK/CLASSIC/LIVE/1977/B02.MP3","dur_s":290,"kbps":112,"seed":15,"title":"Bootleg Seite B","artist":"Unbekannt","album":"Bootleg 1977","track":2,"id3v1":true},
        {"path":"POP/P01.MP3","dur_s":200,"kbps":256,"seed":16,"title":"Sommerhit","artist":"Neon Wellen","album":"Leuchtreklame","track":1,"cover":1},
        {"path":"POP/P02.MP3","dur_s":185,"kbps":256,"seed":17,"title":"Ohrwurm","artist":"Neon Wellen","album":"Leuchtreklame","track":2,"cover":1,"xing":true},
        {"path":"POP/P03.MP3","dur_s":210,"kbps":192,"seed":18,"title":"Refrain Refrain","artist":"Neon Wellen","album":"Leuchtreklame","track":3,"cover":1},
        {"path":"POP/P04.MP3","dur_s":195,"kbps":320,"seed":19,"title":"Radio Edit","artist":"Neon Wellen","album":"Leuchtreklame","track":4,"cover":1,"frag":true},
        {"path":"POP/CHARTS/H01.MP3","dur_s":180,"kbps":320,"seed":20,"title":"Platz Eins","artist":"Diverse","album":"Charts KW 42","track":1,"cover":2},
        {"path":"POP/CHARTS/H02.MP3","dur_s":175,"kbps":128,"seed":21,"title":"Platz Zwei","artist":"Diverse","album":"Charts KW 42","track":2,"cover":3},
        {"path":"POP/CHARTS/H03.MP3","dur_s":190,"kbps":160,"seed":22,"title":"Neueinsteiger","artist":"Diverse","album":"Charts KW 42","track":3,"cover":4,"vbr":true},
        {"path":"POP/CHARTS/H04.MP3","dur_s":205,"kbps":224,"seed":23,"title":"Absteiger","artist":"Diverse","album":"Charts KW 42","track":4,"id3v1":true},
        {"path":"POP/CHARTS/H05.MP3","dur_s":165,"kbps":96,"seed":24,"title":"Wiedereinstieg","artist":"Diverse","album":"Charts KW 42","track":5},
        {"path":"JAZZ/J01.MP3","dur_s":320,"kbps":192,"seed":25,"title":"Blaue Stunde","artist":"Trio Nachtlicht","album":"Spaetschicht","track":1,"cover":3},
        {"path":"JAZZ/J02.MP3","dur_s":280,"kbps":160,"seed":26,"title":"Kontrabass Solo","artist":"Trio Nachtlicht","album":"Spaetschicht","track":2,"cover":3,"xing":true},
        {"path":"JAZZ/J03.MP3","dur_s":360,"kbps":128,"seed":27,"title":"Besen auf Snare","artist":"Trio Nachtlicht","album":"Spaetschicht","track":3,"cover":3},
        {"path":"JAZZ/QUARTET/Q01.MP3","dur_s":240,"kbps":256,"seed":28,"title":"Vier Stimmen","artist":"Quartett Sued","album":"Session 4","track":1,"cover":4},
        {"path":"JAZZ/QUARTET/Q02.MP3","dur_s":300,"kbps":192,"seed":29,"title":"Improvisation","artist":"Quartett Sued","album":"Session 4","track":2,"cover":4,"vbr":true},
        {"path":"JAZZ/QUARTET/Q03.MP3","dur_s":270,"kbps":112,"seed":30,"title":"Ausklang","artist":"Quartett Sued","album":"Session 4","track":3,"id3v1":true},
        {"path":"HOERBUCH/KAPITEL/K01.MP3","dur_s":300,"kbps":96,"seed":31,"title":"Kapitel 1","artist":"Sprecherin M","album":"Die lange Heimfahrt","track":1,"cover":2},
        {"path":"HOERBUCH/KAPITEL/K02.MP3","dur_s":330,"kbps":96,"seed":32,"title":"Kapitel 2","artist":"Sprecherin M","album":"Die lange Heimfahrt","track":2,"cover":2},
        {"path":"HOERBUCH/KAPITEL/K03.MP3","dur_s":290,"kbps":96,"seed":33,"title":"Kapitel 3","artist":"Sprecherin M","album":"Die lange Heimfahrt","track":3,"cover":2},
        {"path":"HOERBUCH/KAPITEL/K04.MP3","dur_s":310,"kbps":96,"seed":34,"title":"Kapitel 4","artist":"Sprecherin M","album":"Die lange Heimfahrt","track":4,"cover":2,"id3v1":true},
        {"path":"PODCAST/E01.MP3","dur_s":420,"kbps":96,"seed":35,"title":"Folge 101 Messtechnik","artist":"Podcast Spur","album":"Spur Podcast","track":101},
        {"path":"PODCAST/E02.MP3","dur_s":390,"kbps":128,"seed":36,"title":"Folge 102 Infotainment","artist":"Podcast Spur","album":"Spur Podcast","track":102,"xing":true},
        {"path":"PODCAST/E03.MP3","dur_s":450,"kbps":96,"seed":37,"no_id3v2":true,"id3v1":true,"title":"Folge 103 nur ID3v1","artist":"Podcast Spur","album":"Spur Podcast","track":103},
        {"path":"MIX/M01.MP3","dur_s":200,"kbps":192,"seed":38,"title":"VBR Mix Eins","artist":"DJ Probe","album":"Wechselraten","track":1,"vbr":true,"cover":1},
        {"path":"MIX/M02.MP3","dur_s":220,"kbps":256,"seed":39,"title":"VBR Mix Zwei","artist":"DJ Probe","album":"Wechselraten","track":2,"vbr":true,"cover":4},
        {"path":"MIX/M03.MP3","dur_s":240,"kbps":128,"seed":40,"title":"VBR Mix Drei","artist":"DJ Probe","album":"Wechselraten","track":3,"vbr":true}]})"},
};
// clang-format on

static constexpr size_t kMediaPresetCount = sizeof(kMediaPresets) / sizeof(kMediaPresets[0]);

/**
 * @brief Look up a built-in preset by its id (exact, case-sensitive match).
 *
 * @param id  Preset id such as "cache_a"; nullptr is allowed.
 * @return Pointer into kMediaPresets, or nullptr if id is null or unknown.
 */
inline const MediaPreset* findMediaPreset(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kMediaPresetCount; i++) {
        if (strcmp(kMediaPresets[i].id, id) == 0) return &kMediaPresets[i];
    }
    return nullptr;
}
