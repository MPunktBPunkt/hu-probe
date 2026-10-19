#pragma once

#include <Arduino.h>

/**
 * Media variant presets (0.2.31). Each preset changes exactly one dimension against
 * its reference so HU behaviour can be attributed (docs/MEDIA_VARIATION.md).
 * `demo_v1` = legacy flash image (FAT12, 3 × identical 158208 B tracks).
 */
struct MediaPreset {
    const char* id;
    const char* label;
    const char* json;  // nullptr = demo image
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
};
// clang-format on

static constexpr size_t kMediaPresetCount = sizeof(kMediaPresets) / sizeof(kMediaPresets[0]);

inline const MediaPreset* findMediaPreset(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kMediaPresetCount; i++) {
        if (strcmp(kMediaPresets[i].id, id) == 0) return &kMediaPresets[i];
    }
    return nullptr;
}
