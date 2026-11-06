// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "JSON.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "benchmark/benchmark.h"

using std::int64_t;
using std::size_t;

using std::string;
using std::string_view;
using std::vector;

using IndicesTy = JSON::SCI::IndicesTy;

namespace {

void JSON_sci_get(benchmark::State &state, string_view json) {
    IndicesTy indices;
    JSON::SCI indexer(json);
    for (auto _ : state) {
        indices = indexer.get_ref();
        benchmark::DoNotOptimize(indices);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(json.size()));
}

#ifdef __ARM_FEATURE_SVE
void JSON_sci_get_sve(benchmark::State &state, string_view json) {
    IndicesTy indices;
    JSON::SCI indexer(json);
    for (auto _ : state) {
        indices = indexer.get_sve();
        benchmark::DoNotOptimize(indices);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(json.size()));
}
#endif // __ARM_FEATURE_SVE

#ifdef __ARM_FEATURE_SVE2
void JSON_sci_get_sve2(benchmark::State &state, string_view json) {
    IndicesTy indices;
    JSON::SCI indexer(json);
    for (auto _ : state) {
        indices = indexer.get_sve2();
        benchmark::DoNotOptimize(indices);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(json.size()));
}
#endif // __ARM_FEATURE_SVE2

#ifdef __ARM_FEATURE_SVE2p2
void JSON_sci_get_sve2p2(benchmark::State &state, string_view json) {
    IndicesTy indices;
    JSON::SCI indexer(json);
    for (auto _ : state) {
        indices = indexer.get_sve2p2();
        benchmark::DoNotOptimize(indices);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(json.size()));
}
#endif // __ARM_FEATURE_SVE2p2

string compact_json(size_t object_count) {
    string json;
    json.reserve(object_count * 64);
    json.push_back('[');
    for (size_t i = 0; i < object_count; ++i) {
        if (i != 0) {
            json.push_back(',');
        }
        json += R"({"id":)";
        json += std::to_string(i);
        json += R"(,"name":"item","active":true,"values":[1,2,3,4]})";
    }
    json.push_back(']');
    return json;
}

string whitespace_string_array_json() {
    string json;
    json.reserve(8192);

    json += "{\n";
    json += R"(  "description": ")";
    json += string(2048, 'x');
    json += "\",\n";
    json += R"(  "numbers": [)";
    for (size_t i = 0; i < 384; ++i) {
        if (i != 0) {
            json += ", ";
        }
        if (i % 16 == 0) {
            json += "\n    ";
        }
        json += std::to_string(i);
    }
    json += "\n  ],\n";
    json += R"(  "records": [)";
    for (size_t i = 0; i < 16; ++i) {
        if (i != 0) {
            json += ",\n";
        }
        json += R"({"label":")";
        json += string(128, static_cast<char>('a' + (i % 26)));
        json += R"(","flags":[true,false,null]})";
    }
    json += "\n  ]\n";
    json += "}\n";

    return json;
}

const string json_empty = "";
const string json_tiny = R"({"a":1,"b":2})";
const string json_small = R"({"a":1,"b":[true,false]})";
const string json_wide_fields =
    R"({"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa":1,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb":2,"cccccccccccccccccccccccccccccccccccccccc":3,"dddddddddddddddddddddddddddddddddddddddd":4,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee":5})";
const string json_compact_kb = compact_json(96);
const string json_whitespace_strings_arrays_kb = whitespace_string_array_json();
const string json_sample = R"([
  -1001124724,
  true,
  false,
  {
    "qwzyoiuost": false,
    "cjopgnu": {
      "foalv": {
        "rlzyv": false,
        "qprwoggp": "4TUcXtlr7uSlaTfFD
        "gfyveu": {
          "iouaemrwi": {
            "wpebctgoer": [
              "H6tAYDl6ykcvMMMvLV",
              [
                true,
                [
                  "H",
                  true,
                  "PPvsZEiPQ_nxC",
                  true,
                  true,
                  false,
                  false,
                  true
                ],
                "Eo9l6",
                [
                  false,
                  "i",
                  2073204919,
                  "EoWiBHbcPZms3ZB-UB"
                  false,
                  "yt",
                  false
                ],
                true,
                [
                  762496935,
                  true,
                  false,
                  "Pu",
                  true,
                  false
                ],
                {
                  "hnyhletcno": 637391
                  "bmnrfqvuu": 1297777
                  "ekbnzknhkno": true,
                  "rhwkxnjy": "Y_gegXE
                },
                "9TR9JAzVHrcSN_v",
                "iMSAXMDrSpOJVe"
              ],
              false,
              -1728634232,
              {
                "nblqeanndgeg": "d8zdk
                "qndxrdox": {
                  "vixot": -732701662.
                  "tynqyfnm": -2117872
                  "agcqlkkipdf": false
                  "dihwc": true
                },
                "utdcqgko": [
                  1310674979,
                  true,
                  -271417947,
                  -1353978037.3672442,
                  false,
                  true,
                  "L_sOscUS4dCTmznz"
                ],
                "fooaohvmja": false,
                "jzhxiwpw": [
                  -673891072,
                  "sk3uiOBdRl-s8",
                  false,
                  1834401848.5266547
                ],
                "lhltj": "0ng"
              },
              "j979TkDy-Rgl9hAYRoQ"
            ],
            "bxoeupgvd": {
              "eeuund": [
                [
                  "zbS7i9SCwAglh"
                ],
                [
                  "wkPDisIfvEICXDNW",
                  "SQD"
                ]
              ],
              "oftgzqlemxdp": 70618395
              "mhaxq": [
                {
                  "phxdahancn": false,
                  "tbxqyajzt": false,
                  "lldlfvo": "NXBvPDFd
                },
                165678095.90106773,
                true,
                {
                  "exyfxbqzgt": -14264
                  "jfumygt": true,
                  "olfpuo": false,
                  "iohayhadu": "Nv",
                  "exbafop": -32955812
                  "onmpwi": 996035002.
                },
                true
              ],
              "tbdktc": true,
              "rcxwlbdffek": "idSN-5L7
            },
            "eirqfemwydxd": [
              "rEYUY72rk681bg5thY-",
              1961069278,
              true,
              [
                [
                  true,
                  -723260859.1632991,
                  -1584627766,
                  true
                ],
                [
                  false
                ],
                "t",
                [
                  false,
                  true,
                  "U_6XKKtCIINO3QuI"
                ],
                "0jfAhTZbx",
                {
                  "mggybusbq": 7284781
                  "rkdeexho": false,
                  "cnqipmb": false,
                  "mjgszqidms": true,
                  "fjtnwk": -119597054
                  "jwbwmnoto": "lDF-ao
                  "npghsxqnnvcd": "sJQ
                  "zehpfwomes": 197036
                },
                [
                  true,
                  -2079247471,
                  "2rXX3D0RLGoTUKSjfdY
                  "a3GrBm",
                  "OoCvHleMPJgxnGpt3I"
                  true,
                  "TDEI",
                  true
                ],
                [
                  "tZuPhAgV",
                  1598020170.370911,
                  -1888926202.3359208,
                  false,
                  1853499837,
                  "6nhki",
                  true,
                  -59089807
                ],
                "IVvC3_N1afJUf-9Qse"
              ],
              [
                [
                  true,
                  "bXcjdgwOM_yJVrBO",
                  1452861133.6821532
                ],
                {
                  "xwrjovqeouad": fals
                  "nmyubaphmief": -164
                  "igyxpinyugbp": "kMN
                  "vmdugiky": 12362818
                  "ehcidfxkkly": false
                  "cnawfggko": false,
                  "anagf": false,
                  "yznxjlqolds": "upJw
                  "mydltq": false
                },
                false
              ],
              {
                "qxysggo": true,
                "spzlmygdb": true,
                "rvxwcahx": {
                  "xmydgwm": false,
                  "tkmny": 699845110,
                  "sxwcbazsfnrp": "lyV
                  "brdvvx": 976953398.
                  "vgqbuzh": -80121493
                  "nyienzx": false
                },
                "appjhzbh": {
                  "eraxjvznv": -924101
                  "xnriool": -18313895
                  "pkiorrbtse": false,
                  "anewrhfm": "zjksYKp
                  "alqmkmg": false,
                  "xkjlzo": -620363703
                  "oiklrdw": "A9_SVvgM
                  "ikefmbuk": "G_Z7TN"
                  "oowzedpt": "L"
                }
              },
              false,
              [
                1649831545,
                [
                  true,
                  1575204926,
                  "DMiXA4Jnaf",
                  "VnpR0VVogc7UfMwCcxS
                  "lJZ0GLYafzRyq"
                ],
                1869324445.6171458,
                {
                  "xebzbkvo": false,
                  "qtssyeewp": false,
                  "drpgnzprvods": true
                  "dgkuyzy": -56493312
                  "bcvulse": "kLt3g",
                  "ikzvzrxai": 6302920
                },
                [
                  -898395704,
                  "C9Vqd_sjpnWIrI",
                  false,
                  "g1-3kwesdt",
                  "kG57JmZrqrwuOLI4oKA
                ]
              ],
              true
            ],
            "wzipyiocbzp": -583275590,
            "lumxngpen": true,
            "pfbudtgzn": true
          },
          "tsbnikgd": true
        },
        "rfjzbsid": 1009720028.8290068
        "kwzggc": "OV29B",
        "qmumrxkizj": 825173927,
        "hazrxng": -505562174
      },
      "hzoklncnddp": "E9yWpPm0eAA7kXQa
      "xrzhg": [
        "f_XpCt6HEFNU",
        "4NX"
      ],
      "cghdyv": false,
      "upqqlpn": false,
      "dvvacycgm": "neaAq1rfDZ1Uag-yPU
      "azytvpnvtgw": "L-lpeKCVr",
      "rpjyj": [
        "etMVB",
        "0vZ0nzJMZ2t5",
        "6205N5MQ9H8ZbEb3FseU",
        true,
        -1209283405.911274,
        "RwNQPvrESg-Q",
        true
      ],
      "mheaxksyyo": {
        "pbdnlojoxo": [
          "Xo4T8P1LF2lp6s3HodrS",
          -1979127950.3656504,
          false,
          [
            [
              [
                -24063525.82313488,
                "BZRBQZ3y0",
                [
                  true,
                  "eCGA",
                  "Kks5TIz_Z",
                  "F7FmhuXBx9wF",
                  979997284.0240536,
                  "FYG2otI",
                  -1241222881
                ]
              ],
              "Vg",
              {
                "ndnfqfodtk": {
                  "pchlvmrv": -1168309
                  "tvrjwezyrf": false,
                  "tvzwlch": true
                },
                "qjcyoim": -954533075.
                "jqnynt": false,
                "gszqacwu": "ZZolhL5ds
                "olmsgl": "lzbseDN8xF"
                "jnyejcjjtja": false,
                "wkfaw": true,
                "qpgfuizzg": "4NWYyonr
                "mcnwjq": {
                  "yezmoweym": true,
                  "kffrwdpuvpc": false
                  "fbszfb": false,
                  "kpnlo": "1dx1LI2ga0
                  "ezqcb": false,
                  "txdulyqbhbp": false
                  "ndloefs": "Ppz3x25n
                  "upsob": -1997322793
                  "fdrgeydbizgz": fals
                }
              },
              true,
              "Eh3sUQ1",
              "zTFZ7bqK5DMb0F8mRDp",
              {
                "vkrxrsxgjeb": true,
                "nclxg": 1491337775,
                "tycse": "owtjEW5A",
                "jyxbppwxi": -10076174
                "iozsk": -1111769868.6
                "uhbfiqbnm": [
                  "EMjM0u7o1fttWjNjt"
                ],
                "ujmxlrpmnw": 19046814
                "mixvwkfkmhsg": "XKe",
                "lhvbmd": [
                  -127288595.66998683,
                  "SvNY0El7rg_x8MS6RiW
                  "Sz6_oMBAl"
                ]
              }
            ],
            2022861583,
            true,
            {
              "gqukbc": {
                "pvscdrskxr": "LDCsefc
                "bdbumzeeujw": {
                  "lkixsahdpp": -12110
                  "ooewgnbxszm": "OJ47
                  "joqjgtem": 41496906
                  "ovguouyqate": -1152
                  "rlqvtgy": "C4LlAwpd
                  "ghdrkukwfjy": 10485
                  "rkfrqmc": "whn1Y_ss
                },
                "fuqwvheafndt": "hcUpu
                "mmmupfikqw": "i_v25Mb
              }
            },
            -1369083206.486973
          ],
          false
        ],
        "xbvtljd": {
          "tdgxlxumhx": 885082806.9822
          "gihvkcak": [
            {
              "mqcaux": 158199382.1133
              "zjnas": true,
              "elxoehpfy": "MwW3EBpnbI
              "bbzfllixz": -1853880993
              "hkmhcyjj": 1277755866.5
            },
            "zAVv3extcY3C9WAQvcW",
            [
              true,
              [
                {
                  "yittjdwusox": 13346
                  "eurjyyf": "6r9lXuDm
                }
              ],
              {
                "youbsyxomxrd": "I",
                "mlhcnuso": "ZupoyCns8
                "fatvm": "25WBGt9hqUG5
                "kfiqugledfqf": {
                  "bvkjvcirbzqs": fals
                },
                "ixyexydke": {
                  "rjtiexndequt": -325
                  "vrnyylykvvq": "QaFd
                  "kalsqbepxjzo": 7724
                  "iwyqt": false
                },
                "wtheufsgfjb": {
                  "nqjhwx": "TNm570",
                  "jlqrc": "gSJ_RQM3gU
                },
                "zvwbcxcc": "9q8GrnMbY
              },
              [
                [
                  142798553.74059477
                ],
                "0a3hME-ZZ2KDjRY1d",
                false,
                true,
                "0a-ypm09IdNNGK9aH"
              ],
              [
                false,
                [
                  "PGTiLFZS6Ir637Vid5W
                  -887881306.3364472,
                  "N5hLZ0YQbBzKyVgO-"
                ]
              ]
            ],
            [
              {
                "aqriymweku": {
                  "zpypmb": 1586715049
                },
                "oaszllra": [
                  true,
                  "0CgtwH",
                  true,
                  "oNMuNyIitzetng0a8RU
                  false
                ],
                "uftosj": [
                  true,
                  116256636.67410165,
                  true,
                  "lHHufFqX3StawV3_",
                  "oBdqluzXdbh8ZsTxs4l
                  "JZ5DMlprNlFbb",
                  "PK2l4Ri9uSHU",
                  false,
                  104357411.14863656
                ],
                "dxwop": true,
                "mdyfbt": [
                  "m3WyAHQCnjYek4OrGfi
                  true,
                  354096773.7692196,
                  true
                ],
                "wprrkqszoa": "uhTi",
                "xkjsf": [
                  -771101198,
                  "eEKibU",
                  "tPFG",
                  -958542753,
                  1752530948,
                  "34WOUf7goOg5V",
                  1255257367.563467,
                  false,
                  88558375.87892757
                ],
                "hagzwnacgkbp": [
                  "JZQJGLAfcbv",
                  "zMs5iIvb",
                  "JJ"
                ],
                "dljtucdpjtxv": [
                  1167257710.1426883,
                  156763639,
                  true,
                  false,
                  "kT6Nk1hnS",
                  1501716011.867569
                ]
              }
            ],
            -1428116912,
            {
              "gpvnlzroiab": [
                "x",
                false,
                true,
                [
                  true,
                  1976562176,
                  -658101440,
                  false,
                  false
                ],
                "bU",
                -260464306.92516604,
                [
                  "35NZmjO",
                  true,
                  true
                ],
                "SvoVG6qsuO"
              ],
              "ratizdtowa": "I1-yoEH2W
              "rsuonq": [
                {
                  "ioeomtd": true,
                  "onxqhn": false,
                  "zogbq": "KM6uB5GlRk
                  "uobvoe": true,
                  "hxvmogfazk": -61671
                  "jgcqlue": true,
                  "qtfwzz": false
                },
                false
              ],
              "fpnepfoudu": false,
              "fstqylog": true
            },
            false
          ],
          "ycmrrqswl": 1515526160,
          "lalnthgnjj": [
            {
              "emvtjlqlyn": "yULP4o0TA
              "tkfwq": false,
              "pjlesnpxjoeo": "bShz4ZC
              "sqmslszdqo": -202615091
              "vosympdop": true
            },
            false,
            {
              "oulan": [
                {
                  "kgzzqhgcyhnk": "ges
                  "aeaqttzrz": "pFq08Z
                  "spdwayjg": true
                },
                false
              ],
              "cqclmut": {
                "ugrzp": "OEmRwSHhVKmW
                "ncmxoybgc": -14179723
                "bkvxjn": "f",
                "xxbnwpm": {
                  "xdsozr": 268274598.
                },
                "vbudms": true,
                "vojmvbfjo": 144525576
                "jbbnlvrtc": "jRKsdJdG
                "fccshn": [
                  "V1-",
                  191504821.68991783,
                  1743963554,
                  false,
                  -1090005652.8108737,
                  914442975,
                  false,
                  "9a7U7zA0qe74u",
                  -1686744869.7010558
                ]
              },
              "gbracjvg": true,
              "zyaejf": [
                1606780027
              ],
              "ohmxgfrumriw": "2Ul8L5A
              "obksjpjxnphj": -8795649
              "jjutstanjday": false,
              "utlcazd": -625502418
            },
            -1662602719,
            {
              "yfvtqyj": {
                "iaggcrsojn": -4049266
                "hwisesbptr": [
                  358086513,
                  true,
                  false,
                  "1OZbwanID0",
                  1545388759,
                  -1141034823,
                  926130636
                ],
                "ajcnsazgewku": [
                  995931265.2501994,
                  true,
                  -589990541.783768,
                  true,
                  -466923181.83344597,
                  "Zp7GiV-WdQ",
                  true,
                  "Rjwq9DQANDnIkNub3",
                  false
                ],
                "iclhyxqju": "tDI7bW4n
                "kdqlqo": "TlQvnvHW5e9
              },
              "zhxcgie": [
                [
                  -1642307471,
                  "J4ew3wVTcaq5J7H0M",
                  "HtpqwDvWb"
                ],
                {
                  "covyhmnfym": "NjRW2
                  "gihire": true
                },
                false,
                [
                  -525851572.46017414,
                  false,
                  false,
                  -538826236.2611501
                ]
              ],
              "mkmrgivm": [
                "RO9gjkq",
                "9cqMLsLNvj3"
              ]
            }
          ],
          "iemrjh": "0WVn1N",
          "uywircvufgw": false,
          "vzwcnw": "WnbovSXaMzb-eyVlL
          "chmfhaju": {
            "wmnmsf": true
          }
        },
        "bxnaymqt": false,
        "pjdwnl": "SzmRZa_Hq",
        "ettpuijywg": {
          "aswyyuais": true,
          "uzkafecbp": "VZtmlngc9GqXby
          "qcdspiqpzwp": [
            [
              1946791168,
              "S15Eh9d0gNrxE",
              false
            ],
            false,
            true,
            -2057045822.726386
          ],
          "eajoz": 759928182.1392952,
          "mvhlqejgjeq": "o1Hwm8mvfnhE
          "oosjbzowynh": [
            "HRANsAjzuRqa6CVa",
            "RvkG_tharES6gsg06nn",
            "1j8IgxlBrZ6lgDT50UF",
            1832583094.8721294,
            false,
            false,
            false
          ]
        },
        "zuyqvi": -693833060,
        "phudh": false,
        "mgnxig": "KG-1gEWmpHm3RxZ",
        "lwtdk": "V96l7"
      }
    },
    "rgffyu": true,
    "tobqt": "coVkh4fkUqkzXh",
    "hzyfwcvfparh": -1280918744.237486
    "qjjzeyhkfcan": {
      "ehgkjmhgthyy": {
        "byentf": 89335836,
        "ajexihh": "tfIPqbxi",
        "qzgseleq": "hC4VipCx5a",
        "wcykzummb": false,
        "ibllwakgbar": true
      }
    },
    "dtqbfdi": 482559477
  }
])";

} // namespace

BENCHMARK_CAPTURE(JSON_sci_get, empty, string_view(json_empty));
BENCHMARK_CAPTURE(JSON_sci_get, tiny, string_view(json_tiny));
BENCHMARK_CAPTURE(JSON_sci_get, small, string_view(json_small));
BENCHMARK_CAPTURE(JSON_sci_get, wide_fields, string_view(json_wide_fields));
BENCHMARK_CAPTURE(JSON_sci_get, compact_kb, string_view(json_compact_kb));
BENCHMARK_CAPTURE(JSON_sci_get, whitespace_strings_arrays_kb,
                  string_view(json_whitespace_strings_arrays_kb));
BENCHMARK_CAPTURE(JSON_sci_get, json_sample, string_view(json_sample));

#ifdef __ARM_FEATURE_SVE
BENCHMARK_CAPTURE(JSON_sci_get_sve, empty, string_view(json_empty));
BENCHMARK_CAPTURE(JSON_sci_get_sve, tiny, string_view(json_tiny));
BENCHMARK_CAPTURE(JSON_sci_get_sve, small, string_view(json_small));
BENCHMARK_CAPTURE(JSON_sci_get_sve, wide_fields, string_view(json_wide_fields));
BENCHMARK_CAPTURE(JSON_sci_get_sve, compact_kb, string_view(json_compact_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve, whitespace_strings_arrays_kb,
                  string_view(json_whitespace_strings_arrays_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve, json_sample, string_view(json_sample));
#endif // __ARM_FEATURE_SVE

#ifdef __ARM_FEATURE_SVE2
BENCHMARK_CAPTURE(JSON_sci_get_sve2, empty, string_view(json_empty));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, tiny, string_view(json_tiny));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, small, string_view(json_small));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, wide_fields,
                  string_view(json_wide_fields));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, compact_kb, string_view(json_compact_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, whitespace_strings_arrays_kb,
                  string_view(json_whitespace_strings_arrays_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve2, json_sample, string_view(json_sample));
#endif // __ARM_FEATURE_SVE2

#ifdef __ARM_FEATURE_SVE2p2
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, empty, string_view(json_empty));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, tiny, string_view(json_tiny));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, small, string_view(json_small));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, wide_fields,
                  string_view(json_wide_fields));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, compact_kb,
                  string_view(json_compact_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, whitespace_strings_arrays_kb,
                  string_view(json_whitespace_strings_arrays_kb));
BENCHMARK_CAPTURE(JSON_sci_get_sve2p2, json_sample, string_view(json_sample));
#endif // __ARM_FEATURE_SVE2p2
